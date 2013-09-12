/*
 * StringSet.h
 *
 *  Created on: May 10, 2012
 *      Author: veselin
 */

#ifndef STRINGSET_H_
#define STRINGSET_H_

#include <stdio.h>
#include <vector>

class StringSet {
public:
	StringSet();

	// Returns the index of the added string.
	int addString(const char* s);

	// Returns the string for an index. The returned pointer is guaranteed
	// to be valid only until the next modification of StringSet.
	const char* getString(int index) const;

	// Returns whether the set contains a given string.
	bool containsString(const char* s) const;

	// Returns the index of a string if exists or -1 otherwise.
	int findString(const char* s);

	// Saves the string set to a file.
	void saveToFile(FILE* f);

	// Loads the string set from a file.
	bool loadFromFile(FILE* f);

private:
	// Returns the index of the added string.
	int addStringL(const char* s, int slen);

	// Returns the index of a string if exists or -1 otherwise.
	int findStringL(const char* s, int slen, int hash) const;

	// Computes hashcode for a string.
	int stringHash(const char* s, int slen) const;

	// Adds a value to the hashtable.
	void addHash(int hash, int value);
	void addHashNoRehash(int hash, int value);

	void rehashAll();

	std::vector<char> m_data;
	std::vector<int> m_hashes;
	int m_hashTableLoad;
};

#endif /* STRINGSET_H_ */
