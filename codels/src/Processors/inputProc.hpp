#ifndef INPUTPROC_HPP
#define INPUTPROC_HPP

#include "../Signals/TimeDomainSignal.hpp"
#include "Processor.hpp"

/* 
 * inputProc is the first processor to receve the audio.
 * It has two time domain signals as output (left and right).
 * As input, it can accept just one dimentinal and continous data
 * per channel.
 * 
 * It has no parameters. As processing, it normalises the input signal.
 * */
 
#include "inputProcLib/inputProcLib.hpp"

namespace openAFE {

	template<typename T>
	class InputProc : public Processor < TimeDomainSignal<T>, TimeDomainSignal<T> > {

		private:

			using PB = Processor<TimeDomainSignal<T>, TimeDomainSignal<T> >;
			
			using typename PB::outT_SignalSharedPtr;
			using typename PB::outT_SignalSharedPtrVector;
			using typename PB::outT_SignalIter;

			using typename PB::outT_nTwoCTypeBlockAccessorPtr;
			using typename PB::outT_nTwoCTypeBlockAccessorPtrVector;
			using typename PB::outT_AccessorIter;
			
			void setDefaultParams () {
				PB::defaultParams.set("Type", "Input Processor");
			}
			
			void setPInfo(const std::string& nameArg,
						  const std::string& labelArg = "Input Processor",
						  const std::string& requestNameArg = "input",
						  const std::string& requestLabelArg = "TimeDomainSignal",
						  const std::string& outputTypeArg = "TimeDomainSignal",
						  unsigned int isBinauralArg = 1						 				 
						) {

				PB::pInfo.name = nameArg;
				PB::pInfo.label = labelArg;
				PB::pInfo.requestName = requestNameArg;
				PB::pInfo.requestLabel = requestLabelArg;
				PB::pInfo.outputType = outputTypeArg;
				PB::pInfo.isBinaural = isBinauralArg;
			}
			
		public:

			/* inputProc */
			InputProc (const std::string& nameArg, const uint64_t fsIn, const uint64_t fsOut, const uint64_t bufferSize_s) : Processor < TimeDomainSignal<T>, TimeDomainSignal<T> > (fsIn, fsOut, _inputProc) {
				
				this->setDefaultParams ();

				/* Extending with default parameters */			
				this->extendParameters ();
				/* Setting the name of this processor and other informations */
				this->setPInfo(nameArg);
				
				/* Creating the output signals */
				outT_SignalSharedPtr leftOutput( new TimeDomainSignal<T>(fsOut, bufferSize_s, this->pInfo.requestName, this->pInfo.name, _left) );
				outT_SignalSharedPtr rightOutput ( new TimeDomainSignal<T>(fsOut, bufferSize_s, this->pInfo.requestName, this->pInfo.name, _right) );
				
				/* Setting those signals as the output signals of this processor */
				PB::outputSignals.push_back( std::move( leftOutput ) );
				PB::outputSignals.push_back( std::move( rightOutput ) );
				
				/* Linking the output accesors of each signal */
				PB::linkAccesors ();
			}
				
			~InputProc () {
				std::cout << "Destructor of a input processor" << std::endl;
			}
			
			/* This function does the asked calculations. 
			 * The inputs are called "privte memory zone". The asked calculations are
			 * done here and the results are stocked in that private memory zone.
			 * However, the results are not publiched yet on the output vectors.
			 */
			void processChunk (T* inChunkLeft, uint64_t leftDim, T* inChunkRight, uint64_t rightDim) {
				
				/* There is just one dimention */
				std::thread leftThread(inputProcLib::normaliseData<T>, inChunkLeft, leftDim);
				std::thread rightThread(inputProcLib::normaliseData<T>, inChunkRight, rightDim);
				
				leftThread.join();                // pauses until left finishes
				rightThread.join();               // pauses until right finishes
			}
						
			/* This funcion publishes (appends) the signals to the outputs of the processor */			
			void appendChunk (T* inChunkLeft, uint64_t leftDim, T* inChunkRight, uint64_t rightDim) {
								
				std::thread leftAppendThread( &TimeDomainSignal<T>::appendTChunk, PB::outputSignals[0], inChunkLeft, leftDim);
				std::thread rightAppendThread( &TimeDomainSignal<T>::appendTChunk, PB::outputSignals[1], inChunkRight, rightDim);
				
				leftAppendThread.join();                // pauses until left finishes
				rightAppendThread.join();               // pauses until right finishes
			}
			
			/* TODO : Resets the internat states. */		
			void reset() {
				PB::reset();
			}							

	}; /* class InputProc */
}; /* namespace openAFE */

#endif /* INPUTPROC_HPP */
