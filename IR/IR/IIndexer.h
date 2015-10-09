#pragma once
#include "IIndex.h"
class IIndexer {
public:
	virtual IIndex* createIndex() = 0;
	virtual IIndexer* setIDictionary(IDictionary *iDictionary) = 0;
	virtual IIndexer* setOutputFilePath(string outputFilePath) = 0;
};