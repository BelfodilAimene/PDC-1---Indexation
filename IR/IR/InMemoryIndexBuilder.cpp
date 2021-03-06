#include "stdafx.h"
#include "InMemoryIndexBuilder.h"
#include "Index.h"
#include "HashTableDictionary.h"
#include "Hasher.h"
#include "DocumentProvider.h"
#include "Tokenizer.h"
#include "StrTkTokenizer.h"
#include "NoCompressor.h"
#include "FileManager.h"

InMemoryIndexBuilder::InMemoryIndexBuilder(string repositoryPath)
{
	InMemoryIndexBuilder::repositoryPath = repositoryPath;
	iDictionary = nullptr;
	iCompressor = nullptr;
	outputFilePath = "";
	iTokenizerType = SIMPLE_TOKENIZER;
}

IIndexBuilder * InMemoryIndexBuilder::setIDictionary(IDictionary * iDictionary)
{
	InMemoryIndexBuilder::iDictionary = iDictionary;
	return this;
}

IIndexBuilder * InMemoryIndexBuilder::setICompressor(ICompressor * iCompressor)
{
	InMemoryIndexBuilder::iCompressor = iCompressor;
	return this;
}

IIndexBuilder * InMemoryIndexBuilder::setITokenizer(int iTokenizerType)
{
	InMemoryIndexBuilder::iTokenizerType = iTokenizerType;
	return this;
}

IIndexBuilder * InMemoryIndexBuilder::setOutputFilePath(string outputFilePath)
{
	InMemoryIndexBuilder::outputFilePath = outputFilePath;
	return this;
}


Index* InMemoryIndexBuilder::createIndex()
{
	if (iDictionary == nullptr) {
		iDictionary = new HashTableDictionary();
	}
	if (iCompressor == nullptr) {
		iCompressor = new NoCompressor();
	}
	DocumentTable * documentTable = new DocumentTable();

	if (outputFilePath.compare("")==0) {
		outputFilePath = DEFAULT_OUTPUT_FILE;
	}
	
	
	IDocumentProvider* documentProvider = new DocumentProvider(repositoryPath);

	Document* document;
	while ((document = documentProvider->getNextDocument()) != nullptr)
	{
		ITokenizer* tokenizer;
		switch (iTokenizerType) {
		case SIMPLE_TOKENIZER:
			tokenizer = new Tokenizer(document);
			break;
		case STRTK_TOKENIZER :
			tokenizer = new StrTkTokenizer(document);
			break;
		default:
			throw runtime_error("No Tokenizer with such ID");
			break;
		}
		string token;
		while ((token = tokenizer->getNextToken()).compare("") != 0)
		{
			addTerm(token, documentTable);
		}
		documentTable->addDocument(document->getDocumentMetaData());
		delete document;
		delete tokenizer;
	}
	delete documentProvider;
	finalize(documentTable);
	Index *index = new Index(iDictionary, documentTable, iCompressor, outputFilePath);
	return index;
}
void InMemoryIndexBuilder::addTerm(string token, DocumentTable *documentTable) {
	Term* term = iDictionary->addTerm(token);
	// The next step is to update the term posting list and document number
	if (term->postingList == nullptr) {
		term->postingList = new list<DocumentTerm>();
		list<DocumentTerm>* postingListAsList = static_cast<list<DocumentTerm>*>(term->postingList);
		DocumentTerm docTerm;
		docTerm.documentIndex = documentTable->getDocumentNumber();
		postingListAsList->insert(postingListAsList->begin(), docTerm);
		term->documentNumber++;

	}
	else {
		list<DocumentTerm>* postingListAsList = static_cast<list<DocumentTerm>*>(term->postingList);
		list<DocumentTerm>::iterator it = postingListAsList->end();
		it--;
		if (documentTable->getDocumentNumber() == it->documentIndex) {
			it->ftd++;
		}
		else {
			DocumentTerm docTerm;
			docTerm.documentIndex = documentTable->getDocumentNumber();
			postingListAsList->insert(postingListAsList->end(), docTerm);
			term->documentNumber++;
		}
	}
}
void InMemoryIndexBuilder::finalize(DocumentTable * documentTable)
{

		documentTable->finalize();
		// write the posting list on file and change the pointer to offset in file (invertedFilePath)
		// the offstream structure file is :
		//		Dictionary offset in this file
		//		Terms number in Dictionary
		//		Compressor ID
		//		DocumentMetaDatas number in DocumentTable
		//		DocumentMetaDatas
		//		posting lists
		//		Dictionary
		ofstream *outputFile = FileManager::openOfstream(outputFilePath);
			

		// prepare a place for the dictionary offset
		unsigned int offsetPlacer = 0;
		outputFile->write((const char *)&offsetPlacer, sizeof(unsigned int));

		// write the terms number in HashTableDictionary
		outputFile->write((const char *)&iDictionary->getTermsNumber(), sizeof(unsigned long long));

		// write the Compressor ID
		outputFile->write((const char *)&iCompressor->getCompressorId(), sizeof(int));

		// write the DocumentMetaDatas number in DocumentTable
		outputFile->write((const char *)&documentTable->getDocumentNumber(), sizeof(unsigned int));

		// write the DocumentMetaDatas
		outputFile->write((const char *)documentTable->getFinalizedDocumentTable(), documentTable->getDocumentNumber()*sizeof(DocumentMetaData));

		// write the posting lists contiguously
		IIterator* termIterator = iDictionary->getIterator();
		Term * term;
		int i = 0;
		while ((term = static_cast<Term*>(termIterator->getNext())) != nullptr) {
			list<DocumentTerm>* postingListAsList = static_cast<list<DocumentTerm>*>(term->postingList);
			unsigned int  postingListOffset = (unsigned int)outputFile->tellp();
			iCompressor->compressAndWrite(outputFile, postingListAsList);
			postingListAsList->erase(postingListAsList->begin(), postingListAsList->end());
			delete term->postingList;
			term->postingList = (void *)postingListOffset;
			i++;
		}
		delete termIterator;

		// save the dictionary offset
		unsigned int  dictionaryOffset = (unsigned int)outputFile->tellp();

		// write the terms of the dictionary
		termIterator = iDictionary->getIterator();
		while ((term = static_cast<Term*>(termIterator->getNext())) != nullptr) {
			*outputFile << term;
		}
		outputFile->seekp(0);
		outputFile->write((const char *)&dictionaryOffset, sizeof(unsigned int));
		outputFile->close();
		delete termIterator;
		delete outputFile;
		
}



InMemoryIndexBuilder::~InMemoryIndexBuilder()
{

}
