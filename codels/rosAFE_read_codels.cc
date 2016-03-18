#include "stateMachine.hpp"
#include "Ports.hpp"

using namespace openAFE;

/* --- getAudioData ----------------------------------------------------- */

/* This function takes samples from the structure pointed by src
   (input data from the server's port), and copies them at the
   locations pointed by destL and destR. Note that destL and destR
   should point to allocated memory.

   It copies N samples at most from each channel (left and right
   sequence members of *src), starting at the index *nfr (next frame
   to read).

   The return value n is the amount of samples that the function was
   actually able to get (n <= N).

   If loss is not NULL, the function stores the amount of frames that
   were lost in *loss (*loss equals 0 if no loss).
 */

int getAudioData(binaudio_portStruct *src, inputT *destL, inputT *destR,
                 int N, int64_t *nfr, int *loss)
{
    int n;       /* amount of frames the function will be able to get */
    int fop;     /* total amount of Frames On the Port */
    int64_t lfi; /* Last Frame Index on the port */
    int64_t ofi; /* Oldest Frame Index on the port */
    int pos;     /* current position in the data arrays */

    fop = src->nFramesPerChunk * src->nChunksOnPort;
    lfi = src->lastFrameIndex;
    ofi = (lfi-fop+1 < 0 ? 0 : lfi-fop+1);

    /* Detect a data loss */
    if (loss) *loss = 0;
    if (*nfr < ofi) {
        if (loss) *loss = ofi - *nfr;
        *nfr = ofi;
    }

    /* Compute the starting position in the left and right input arrays */
    pos = fop - (lfi - *nfr + 1);

    /* Fill the output arrays l and r */
    for (n = 0; n < N && pos < fop; ++n, ++pos, ++*nfr) {
        destL[n] = src->left._buffer[pos];
        destR[n] = src->right._buffer[pos];
    }

    return n;
}

/* --- Task read -------------------------------------------------------- */

/* --- Activity GetBlocks ----------------------------------------------- */

/* Variables shared between the codels (they could go in the IDS) */
static int N;
static unsigned int globalLoss;
static int64_t nfr;
static inputT *li, *ri;
std::vector<inputT> l, r;

/** Codel startGetBlocks of activity GetBlocks.
 *
 * Triggered by rosAFE_start.
 * Yields to rosAFE_waitExec.
 * Throws rosAFE_e_noData.
 */
genom_event
startGetBlocks(const char *name, uint32_t nFramesPerBlock,
               int32_t startOffs, uint32_t bufferSize_s,
               rosAFE_inputProcessors **inputProcessorsSt,
               const rosAFE_Audio *Audio,
               const rosAFE_inputProcessorOutput *inputProcessorOutput,
               genom_context self)
{	
  /* Check if the client can get data from the server */
  Audio->read(self);
  if (Audio->data(self) == NULL) {
      printf("The server is not streaming or the port is not connected.\n");
      return rosAFE_e_noData(self);
  }
    
  inputProcPtr inputP ( new InputProc<inputT>( name, Audio->data(self)->sampleRate, Audio->data(self)->sampleRate, bufferSize_s) );
  
  /* Adding this procesor to the ids */
  (*inputProcessorsSt)->processorsAccessor->addProcessor( inputP );
  
  /* Initialization */
  N = nFramesPerBlock; //N is the amount of frames the client requests
  nfr = startOffs + Audio->data(self)->lastFrameIndex + 1;
  l.resize(N); // l and r are arrays containing the
  r.resize(N); // current block of data

  li = l.data(); ri = r.data(); // li and ri point to the current position in the block

  /* Initialization of the output port */
  initInputPort( inputProcessorOutput, self );
  
  globalLoss = 0;
     
  return rosAFE_waitExec;
}

/** Codel waitExecGetBlocks of activity GetBlocks.
 *
 * Triggered by rosAFE_waitExec.
 * Yields to rosAFE_pause_waitExec, rosAFE_exec, rosAFE_stop.
 * Throws rosAFE_e_noData.
 */
genom_event
waitExecGetBlocks(uint32_t *nBlocks, uint32_t nFramesPerBlock,
                  const rosAFE_Audio *Audio, genom_context self)
{
    binaudio_portStruct *data;
    int n, loss;

    /* Read data from the input port */
    Audio->read(self);
    data = Audio->data(self);

    /* Get N frames from the Audio port. getAudioData returns the
       amount of frames n it was actually able to get. The amount N
       required by the client is updated, as well as li and ri */
    n = getAudioData(data, li, ri, N, &nfr, &loss);
    // printf("Requested %6d frames, got %6d.\n", N, n);
    N -= n; li += n; ri += n;

    /* The client deals with data loss here */
    // if (loss > 0) printf("!!Lost %d frames!!\n", loss);
	globalLoss += loss;
    /* If the current block is not complete, call getAudioData again
       to request the remaining part */
    if (N > 0) return rosAFE_pause_waitExec;

    /* The current block is complete. Reset N, li and ri for next block */
    N = nFramesPerBlock; li = l.data(); ri = r.data();
    
    if ( ( globalLoss >= l.size() ) || ( globalLoss >= r.size() ) ) {
	/* Everythink is lost */
		globalLoss = 0;		
		return rosAFE_pause_waitExec;
	}

    if (*nBlocks == 0) return rosAFE_exec;
    if (--*nBlocks > 0) return rosAFE_exec;
    return rosAFE_stop;
}

/** Codel execGetBlocks of activity GetBlocks.
 *
 * Triggered by rosAFE_exec.
 * Yields to rosAFE_waitRelease, rosAFE_stop.
 * Throws rosAFE_e_noData.
 */
genom_event
execGetBlocks(const char *name,
              const rosAFE_inputProcessors *inputProcessorsSt,
              genom_context self)
{
  /* The client processes the current block l and r here */
  inputProcessorsSt->processorsAccessor->getProcessor( name )->processChunk( l.data(), l.size() - globalLoss, r.data(), r.size() - globalLoss);
  std::cout << name << std::endl;  
  return rosAFE_waitRelease;
}

/** Codel waitReleaseGetBlocks of activity GetBlocks.
 *
 * Triggered by rosAFE_waitRelease.
 * Yields to rosAFE_pause_waitRelease, rosAFE_release, rosAFE_stop.
 * Throws rosAFE_e_noData.
 */
genom_event
waitReleaseGetBlocks(const char *name, rosAFE_flagMap **flagMapSt,
                     genom_context self)
{      
  /* Waiting for all childs */
  if ( ! SM::checkFlag( name, flagMapSt, self) )
	return rosAFE_pause_waitRelease;  

  /* Rising the flag (if any) */
  SM::riseFlag ( name, flagMapSt, self);
  					
  // ALL childs are done
  return rosAFE_release;}

/** Codel releaseGetBlocks of activity GetBlocks.
 *
 * Triggered by rosAFE_release.
 * Yields to rosAFE_pause_waitExec, rosAFE_stop.
 * Throws rosAFE_e_noData.
 */
genom_event
releaseGetBlocks(const char *name,
                 rosAFE_inputProcessors **inputProcessorsSt,
                 rosAFE_flagMap **newDataMapSt,
                 const rosAFE_inputProcessorOutput *inputProcessorOutput,
                 genom_context self)
{
  /* Relasing the data */
  (*inputProcessorsSt)->processorsAccessor->getProcessor( name )->appendChunk( l.data(), l.size() - globalLoss, r.data(), r.size() - globalLoss );
  (*inputProcessorsSt)->processorsAccessor->getProcessor( name )->calcLastChunk( );

  /* Publishing on the output port */
  publishInputPort( inputProcessorOutput, self );
  
  /* Informing all the potential childs to say that this is a new chunk. */
  SM::riseFlag ( name, newDataMapSt, self);
  
  globalLoss = 0;
  return rosAFE_pause_waitExec;
}

/** Codel stopGetBlocks of activity GetBlocks.
 *
 * Triggered by rosAFE_stop.
 * Yields to rosAFE_ether.
 * Throws rosAFE_e_noData.
 */
genom_event
stopGetBlocks(rosAFE_inputProcessors **inputProcessorsSt,
              rosAFE_flagMap **flagMapSt,
              rosAFE_flagMap **newDataMapSt, genom_context self)
{
	// l.clear(); r.clear();
	
	/* Delting all input processors (even if there could be just one) */
    (*inputProcessorsSt)->processorsAccessor->clear();
	
	/* Delting all flags */    
    (*flagMapSt)->allFlags.clear();
    (*newDataMapSt)->allFlags.clear();
    
	delete (*inputProcessorsSt);
	
	delete (*flagMapSt);
	delete (*newDataMapSt);

    return rosAFE_ether;
}
