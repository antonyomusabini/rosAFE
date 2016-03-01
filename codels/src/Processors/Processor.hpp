#ifndef PROCESSOR_HPP
#define PROCESSOR_HPP

#include <apf/parameter_map.h>

#include <assert.h>
#include <memory>
#include <thread>
#include <iostream>

namespace openAFE {

	using apfMap = apf::parameter_map;
	typedef apfMap::const_iterator mapIterator;

	/* The type of the processing */
	enum procType {
		_inputProc,
		_preProc
	};
	
	/* Processor Info struct */            
	struct pInfoStruct {
		std::string name;
		std::string label;
		std::string requestName;
		std::string requestLabel;
		std::string outputType;
		unsigned int isBinaural;		// Flag indicating the need for two inputs
	};
	
	/* The father class for all processors in rosAFE */
	template<typename inT, typename outT>
	class Processor {
		public:
			typedef std::shared_ptr<inT > 													inT_SignalSharedPtr;
			typedef std::vector<inT_SignalSharedPtr > 										inT_SignalSharedPtrVector;
			typedef typename std::vector<inT_SignalSharedPtr >::iterator 					inT_SignalIter;

			typedef std::shared_ptr<outT > 													outT_SignalSharedPtr;
			typedef std::vector<outT_SignalSharedPtr > 										outT_SignalSharedPtrVector;
			typedef typename std::vector<outT_SignalSharedPtr >::iterator 					outT_SignalIter;

			typedef typename inT::nTwoCTypeBlockAccessorPtr 								inT_nTwoCTypeBlockAccessorPtr;			
			typedef std::vector<inT_nTwoCTypeBlockAccessorPtr > 							inT_nTwoCTypeBlockAccessorPtrVector;
			typedef typename std::vector<inT_nTwoCTypeBlockAccessorPtrVector >::iterator 	inT_AccessorIter;
			
			typedef typename outT::nTwoCTypeBlockAccessorPtr 								outT_nTwoCTypeBlockAccessorPtr;
			typedef std::vector<outT_nTwoCTypeBlockAccessorPtr >	 						outT_nTwoCTypeBlockAccessorPtrVector;
			typedef typename std::vector<outT_nTwoCTypeBlockAccessorPtrVector >::iterator 	outT_AccessorIter;
		private:			
			
			/* This function fills the defaultParams map with default
			 * parameters for each processor
			 * */
			virtual void setDefaultParams () = 0;
			
			/* This function fills the pInfoStruct of the processor */
			virtual void setPInfo(const std::string& nameArg,
						  const std::string& labelArg,
						  const std::string& requestNameArg,
						  const std::string& requestLabelArg,
						  const std::string& outputTypeArg,
						  unsigned int isBinauralArg ) = 0;
									
		protected:
		
			inT_SignalSharedPtrVector inputSignals;
			outT_SignalSharedPtrVector outputSignals;

			inT_nTwoCTypeBlockAccessorPtrVector inT_lastChunkInfo, inT_lastDataInfo, inT_oldDataInfo, inT_wholeBufferInfo;
			outT_nTwoCTypeBlockAccessorPtrVector outT_lastChunkInfo, outT_lastDataInfo, outT_oldDataInfo, outT_wholeBufferInfo;
						
			apfMap processorParams;				// The parameters used by this processor
			apfMap defaultParams;				// The default parameters of this processor
			stringVector blacklist;				// The parameters which are not allowed to be modified in real time
			
			uint64_t fsIn;						// Sampling frequency of input (i.e., prior to processing)
			uint64_t fsOut;			 			// Sampling frequency of output (i.e., resulting from processing)

			procType type;						// The type of this processing
			pInfoStruct pInfo;					// The informations of this processor
			
			/* Output signas are conserved in this vector.
			 * This is a shared pointer vecor and not a unique pointer vector, because
			 * the same pointer will be placed in dataObject too.
			 */
			//signalBaseSharedPtrVector outputSignals;
			
			//bool isBinaural;         			// Flag indicating the need for two inputs
			bool hasTwoOutputs = false; 		// Flag indicating the need for two outputs
			// channel Channel;					// On which channel (left, right or mono) the processor operates
			
			/* Wont be included in rosAFE, but exists in Matlab AFE : why ? */
			// bool bHidden = false;            	// Set to true to hide the processor from the framework

           /* This method is called at setup of a processor parameters, to verify that the
             * provided parameters are valid, and correct conflicts if needed.
             * Not needed by many processors, hence is not made virtual pure, but need to be
             * overriden in processors where it is needed.
             * */
			virtual void verifyParameters() {}
			
			/* Add default value(s) to missing parameters in a processor */
			void extendParameters() {
				for(mapIterator it = this->defaultParams.begin(); it != this->defaultParams.end(); it++)
					if ( this->processorParams.has_key( it->first ) == 0 )
						this->processorParams.set( it->first, it->second );
					// else do nothing
			}
			
            /* Returns the list of parameters that cannot be affected in real-time. Most
             * (all) of them cannot be modified as they involve a change in dimension of
             * the output signal. A work-around allowing changes in dimension might be
             * implemented in the future but remains a technical challenge at the moment.
             * 
             * To modify one of these parameters, the corresponding processor should be
             * deleted and a new one, with the new parameter value, instantiated via a 
             * user request.
             */
			void setBlacklistedParameters() {
				Processor::blacklist.push_back( "fb_type" );
				Processor::blacklist.push_back( "fb_lowFreqHz" );
				Processor::blacklist.push_back( "fb_highFreqHz" );
				Processor::blacklist.push_back( "fb_nERBs" );
				Processor::blacklist.push_back( "fb_cfHz" );				
			}
			
			stringVector& getBlacklistedParameters() {
				return this->blacklist;
			}
			
			void linkAccesors () {
				unsigned int signalNumber = outputSignals.size();
				outT_lastChunkInfo.reserve(signalNumber); outT_lastDataInfo.reserve(signalNumber);
				outT_oldDataInfo.reserve(signalNumber); outT_wholeBufferInfo.reserve(signalNumber);

				for(outT_SignalIter it = outputSignals.begin() ; it != outputSignals.end() ; ++it) {
					outT_lastChunkInfo.push_back ( (*it)->getLastChunkAccesor() );
					outT_lastDataInfo.push_back ( (*it)->getLastDataAccesor() );
					outT_oldDataInfo.push_back ( (*it)->getOldDataAccesor() );
					outT_wholeBufferInfo.push_back ( (*it)->getWholeBufferAccesor() );
				}
			}

		public:
					
			/* PROCESSOR Super-constructor of the processor class
			  * 
			  * INPUT ARGUMENTS:
			  * fsIn : Input sampling frequency (Hz)
			  * fsOut : Output sampling frequency (Hz)
			  * procName : Name of the processor to implement
			  * parObj : Parameters instance to use for this processor
			  */
			Processor (const uint64_t fsIn, const uint64_t fsOut, procType typeArg) {

				this->setBlacklistedParameters();
				
				this->fsIn = fsIn;
				this->fsOut = fsOut;
				this->type = typeArg;
											
			}
			
			~Processor () {
				inputSignals.clear();
				outputSignals.clear();
				std::cout << "Destructor of Processor class named as : '" << pInfo.name << "'" << std::endl;		
			}
			
			/* PROCESSOR abstract methods (to be implemented by each subclasses): */
			/* PROCESSCHUNK : Returns the output from the processing of a new chunk of input */
			// virtual void processChunk () {} // = 0;
			/* RESET : Resets internal states of the processor, if any */
			virtual void reset () {
				for(outT_SignalIter it = outputSignals.begin() ; it != outputSignals.end() ; ++it)
					(*it)->reset();
			}

			/* GETCURRENTPARAMETERS  This methods returns a list of parameter
			 * values used by a given processor. */
			apfMap& getCurrentParameters() {
				return this->processorParams;
			}
			
			/* HASPARAMETERS Test if the processor uses specific parameters */
			bool hasParameters(const std::string& parName) {
				return processorParams.has_key( parName );
			}
            
			/* MODIFYPARAMETER Requests a change of value of one parameter */
			void modifyParameter(std::string parName, std::string newValue) {
				if ( defaultParams.has_key( parName ) )
					this->processorParams.set( parName, newValue );
				// else std::cout << "parName is not a parameter of this processor" std::endl;
			}

			/* Returns a pointer to the type of this processor */		
			procType* getType () {
				return &type;
			}
			
			/* Returns a pointer to the infos struct of this processor */
			pInfoStruct* getProcessorInfo() {
				return &pInfo;
			}

			/* Getter methods : This funtion sends a const reference for the asked parameter's value */
			const std::string& getParameter( std::string paramArg ) {
				return Processor::processorParams[ paramArg ];
			}
			
			/* Compare only the information of the two processors */
			bool compareInfos (const Processor& toCompare) {
				if ( this->pInfo.name == toCompare.pInfo.name )
					if ( this->pInfo.label == toCompare.pInfo.label )
						if ( this->pInfo.requestName == toCompare.pInfo.requestName )
							if ( this->pInfo.requestLabel == toCompare.pInfo.requestLabel )
								if ( this->pInfo.outputType == toCompare.pInfo.outputType )
									if ( this->pInfo.isBinaural == toCompare.pInfo.isBinaural )
										return true;
				return false;
			}
			
			/* Comapres informations and the current parameters of two processors */
			bool operator==(const Processor& toCompare) {
				if ( this->compareInfos( toCompare ) )
					if ( this->processorParams == toCompare.processorParams )
						return true;
				return false;	
			}

			void printSignals() {
				for(outT_SignalIter it = outputSignals.begin(); it != outputSignals.end(); ++it)
					(*it)->printSignal( );
			}

			void calcLastChunk() {
				for(outT_SignalIter it = outputSignals.begin(); it != outputSignals.end(); ++it)
					(*it)->calcLastChunk( );
			}

			// FIXME : this may be not useful as it is. Think about it.
			uint64_t getFreshDataSize() {
				outT_SignalIter it = outputSignals.begin(); 
				return (*it)->getFreshDataSize();
			}
			
			void calcLastData( uint64_t samplesArg ) {
				for(outT_SignalIter it = outputSignals.begin(); it != outputSignals.end(); ++it)
					(*it)->calcLastData( samplesArg );
			}
			
			void calcOldData( uint64_t samplesArg = 0 ) {
				for(outT_SignalIter it = outputSignals.begin(); it != outputSignals.end(); ++it)
					(*it)->calcOldData( samplesArg );
			}

			void calcWholeBuffer() {
				for(outT_SignalIter it = outputSignals.begin(); it != outputSignals.end(); ++it)
					(*it)->calcWholeBuffer( );
			}

			outT_nTwoCTypeBlockAccessorPtrVector& getLastChunkAccesor( ) {
				return this->outT_lastChunkInfo;
			}

			outT_nTwoCTypeBlockAccessorPtrVector& getLastDataAccesor( ) {
				return this->outT_lastDataInfo;
			}
			
			outT_nTwoCTypeBlockAccessorPtrVector& getOldDataAccesor( ) {
				return this->outT_oldDataInfo;
			}
			
			outT_nTwoCTypeBlockAccessorPtrVector& getWholeBufferAccesor( ) {
				return this->outT_wholeBufferInfo;
			}
											
	};
	
};


#endif /* PROCESSOR_HPP */
