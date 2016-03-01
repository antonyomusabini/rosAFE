#ifndef SIGNAL_HPP
#define SIGNAL_HPP

#include <stdint.h>
#include <vector>
#include <string> 

#include <iostream> 
#include <memory>

#include <apf/parameter_map.h>

#include "circularContainer.hpp"

namespace openAFE {
	
	enum channel {
		_mono,
		_left,
		_right
	};
	  
	template<typename T>
	class Signal {

	public:
		typedef std::shared_ptr<nTwoCTypeBlockAccessor<T> > nTwoCTypeBlockAccessorPtr;
	    typedef std::shared_ptr<CircularContainer<T> > containerPtr;
	    		
	private:

	    typedef std::vector<containerPtr > bufferPtrVector;
	    typedef typename std::vector<containerPtr >::iterator bufferIter;

	protected:

		stringP Label;           			// Used to label the signal
		stringP Name;            			// Used as an instance name
		stringP Dimensions;      			// String describing the dimensions of the signal
		unsigned int  FsHz;       			// Sampling frequency
		channel Channel;         			// Flag keeping track of the channel

		uint64_t bufferSizeSamples;
		uint64Vector bufferElemSize;
		
		bufferPtrVector bufferVector;		// The buffer(s) of the signal
		nTwoCTypeBlockAccessorPtr allLastChunkInfo, allLastDataInfo, allOldDataInfo, allWholeBufferInfo;
		
		void populateProperties(apf::parameter_map& params) {
			
			if (params.has_key("Label"))
				Label.reset( new std::string(params["Label"]) );
			if (params.has_key("Name"))
				Name.reset( new std::string(params["Name"]) );
			if (params.has_key("Dimensions"))
				Dimensions.reset( new std::string(params["Dimensions"]) );
		}

		void clearData() {
			for(bufferIter it = bufferVector.begin(); it != bufferVector.end(); ++it)
				(*it)->clear();
		}
				

	public:

		/*	
		 *  INPUT ARGUMENTS:
		 *  fs : Sampling frequency (Hz)
		 *  bufferSize_s : Buffer duration in s
		 *  bufferElemSize : Additional dimensions of the buffer
		 *                   [dim2,dim3,...]
		*/
				      
		Signal(const uint64_t fs, const uint64_t bufferSize_s, const uint64Vector& bufferElemSize = uint64Vector() ) {
		
		/*
		 *  If you call the constructor with just two arguments, it is automatically a vector.
		 * 	If you want 2D, 3D, 4D .. matrixes, initialise the bufferElemSize when you
		 *  call the constructor
		 */
			this->FsHz = fs;
			
			/* Calculation of the buffer size in samples */
			this->bufferSizeSamples = ceil( bufferSize_s * this->FsHz );
			this->bufferElemSize = bufferElemSize;
			
			this->init();
		}    
		
		~Signal() {
			// Buf, Label, Name and Dimensions are smart pointrs
			// this->bufferVector.clear();
			std::cout << "Destructor of a Signal<T>" << std::endl;
		}
		
		void init() {
			
			/* bufferNumber = number of buffer to create */
			unsigned int bufferNumber = 1;
			if ( bufferElemSize.size() > 0 ) {
				if ( bufferElemSize.size() > 1 ) {
				for (uint64Vector::const_iterator it = bufferElemSize.begin() ; it != bufferElemSize.end(); ++it)
				 bufferNumber *=  *it;
				} else bufferNumber = *( bufferElemSize.begin() );
			}
			
			/* Buffer's pointers are stored linearly.
			 * As itearators are used, this is not mendatory. */
			this->bufferVector.reserve(bufferNumber);
			
			/* Instantiation of the 1D buffers. Buffers are smart (unique) pointers */
			for (unsigned int i = 0 ; i < bufferNumber ; ++i) {
				containerPtr newBuff( new CircularContainer<T>(bufferSizeSamples) );
				bufferVector.push_back ( newBuff );
			}
			
			/* 
			 * C type pointer version for allLastChunkInfo,
			 * allLastDataInfo and for allOldDataInfo
			 * */
			
			/* This struct is to acces to the last chunk of each buffer */
			this->allLastChunkInfo.reset( new nTwoCTypeBlockAccessor<T>( bufferNumber ) );

			/* This struct is to acces to the last data of asked sample numbers of each buffer */			
			this->allLastDataInfo .reset ( new nTwoCTypeBlockAccessor<T>( bufferNumber ) ) ;

			/* This struct is to acces to the oldest data of asked sample numbers of each buffer */	
			this->allOldDataInfo.reset ( new nTwoCTypeBlockAccessor<T>( bufferNumber ) ) ;
	
			this->allWholeBufferInfo.reset ( new nTwoCTypeBlockAccessor<T>( bufferNumber ) ) ;		
		}

		uint64_t getBufferNumber() {
			return this->bufferVector.size();
		}
				
		void reset () {
			for(bufferIter it = bufferVector.begin(); it != bufferVector.end(); ++it)
				(*it)->reset();
		}

		/* When setting a Signal, if the inChunk is pointed to the data of 
		 * this signal, you will face with an error. This is not possible !
		 */ 		
		virtual void setData(nTwoCTypeBlockAccessorPtr inChunk) {	
			this->clearData();
			this->bufferVector.clear();
			this->init();
			appendChunk( inChunk );	
		}
		
		/* appendChunk : In the second case, the data is conserved in two continious
		 * c type arrays. But the two arrays migth not be continuous.
		 * This is due to the outcome of the circular buffer
		 * (look to the circularContainer.hpp to more information).
		 * In this case the two parts of the signal should be appended
		 * one after the other.
		 */
		void appendChunk( nTwoCTypeBlockAccessorPtr inChunk ) {
			
			assert( inChunk->getDimOfSignal() == bufferVector.size() );
			uint64_t i = 0;
			
			for(bufferIter it = bufferVector.begin(); it != bufferVector.end(); ++it)
				(*it)->push_chunk( inChunk->getTwoCTypeBlockAccessor( i++ ) ) ;
		}
		
		/* Append one dimentional cotinious data */
		void appendChunk( T* inChunk, uint64_t dim, uint64_t bufferNumber = 0 ) {
			assert( bufferNumber <= bufferVector.size() );
			bufferIter it = bufferVector.begin();
				
			/* The first array */
			( *(it + bufferNumber) )->push_chunk( inChunk, dim );

		}
				
		stringP getName(){
			return Name;
		}
		
		/* getChannel : returns a pointer to the channel of this signal */
		channel* getChannel() {  // try const
			return &Channel;
		}
		
		/* printSignal() : prints this signals informatinos */		
		virtual void printSignal() {
			std::cout << "Label : " << *(this->Label) << std::endl;
			std::cout << "Name : " << *(this->Name) << std::endl;	
			std::cout << "Dimensions : " << *(this->Dimensions) << std::endl;
			std::cout << "FsHz : " << (this->FsHz) << std::endl;				
			std::cout << "Channel : " << (this->Channel) << std::endl;
		}
		
		nTwoCTypeBlockAccessorPtr getLastChunkAccesor() {
			return this->allLastChunkInfo;
		}

		nTwoCTypeBlockAccessorPtr getLastDataAccesor( ) {
			return this->allLastDataInfo;
		}
		
		nTwoCTypeBlockAccessorPtr getOldDataAccesor( ) {
			return this->allOldDataInfo;
		}

		nTwoCTypeBlockAccessorPtr getWholeBufferAccesor() {
			return this->allWholeBufferInfo;
		}
		
		// FIXME : this may be not useful as it is. Think about it.
		uint64_t getFreshDataSize() {
			bufferIter it = bufferVector.begin(); 
			return (*it)->getFreshDataSize();
		}

		void calcLastChunk() {
			uint64_t i = 0;
			for(bufferIter it = bufferVector.begin(); it != bufferVector.end(); ++it) {
				(*it)->calcLastChunk();
				allLastChunkInfo->setData ( i++, (*it)->getLastChunkAccesor() );
			}
		}

		void calcLastData( uint64_t samplesArg ) {
			uint64_t i = 0;
			for(bufferIter it = bufferVector.begin(); it != bufferVector.end(); ++it) {
				(*it)->calcLatestData( samplesArg );
				allLastDataInfo->setData ( i++, (*it)->getLastDataAccesor( ) );
			}
		}
		
		void calcOldData( uint64_t samplesArg = 0 ) {
			uint64_t i = 0;
			for(bufferIter it = bufferVector.begin(); it != bufferVector.end(); ++it) {
				(*it)->calcOldData( samplesArg );
				allOldDataInfo->setData ( i++, (*it)->getOldDataAccesor( ) );
			}
		}

		void calcWholeBuffer() {
			uint64_t i = 0;
			for(bufferIter it = bufferVector.begin(); it != bufferVector.end(); ++it) {
				(*it)->calcWholeBuffer( );
				allWholeBufferInfo->setData ( i++, (*it)->getWholeBufferAccesor() );
			}
		}
		
	    
	}; /* class Signal */
}; /* namespace openAFE */

#endif /* SIGNAL_HPP */

