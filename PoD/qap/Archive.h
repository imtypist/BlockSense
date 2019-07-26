#pragma once

#include "Archiver.h"
#include "Encoding.h"

// Wrapper class that takes generic types and reads/write them to an archiver
// Note: Don't try to write(ptr) or you'll transmit the pointer value, rather than what it points to!

class Archive {
public:
	Archive(Archiver* arc, Encoding* encoding) : arc(arc), encoding(encoding) { ; }
	
	template <typename Type>
	void write(Type& item);

	template <typename Type>
	void write(Type* item, int count);

		
	template <typename Type>
	void read(Type& item);

	template <typename Type>
	void read(Type* item, int count);

private:
	Archiver* arc;
	Encoding* encoding;
};


template <typename Type>
void Archive::write(Type& item) {
	arc->write((unsigned char*)&item, sizeof(Type));
}

template <typename Type>
void Archive::write(Type* item, int count) {
	arc->write((unsigned char*)item, sizeof(Type) * count);
}

template <typename Type>
void Archive::read(Type& item) {
	arc->read((unsigned char*)&item, sizeof(Type));
}

template <typename Type>
void Archive::read(Type* item, int count) {
	arc->read((unsigned char*)item, sizeof(Type) * count);
}


#ifdef CONDENSE_ARCHIVES

// Specialize the templated functions to compress/decompress EC points

template <>
inline void Archive::write(LEncodedElt& item) {
	LCondensedEncElt elt;
	encoding->compress(item, elt);
	arc->write((unsigned char*)&elt, sizeof(LCondensedEncElt));
}

template <>
inline void Archive::write(LEncodedElt* item, int count) {
	LCondensedEncElt* elts = new LCondensedEncElt[count];
	encoding->compressMany(item, elts, count, false);
	arc->write((unsigned char*)elts, sizeof(LCondensedEncElt) * count);
	delete [] elts;
}

template <>
inline void Archive::read(LEncodedElt& item) {
	LCondensedEncElt elt;
	arc->read((unsigned char*)&elt, sizeof(LCondensedEncElt));
	encoding->decompress(elt, item);
}

template <>
inline void Archive::read(LEncodedElt* item, int count) {
	LCondensedEncElt* elts = new LCondensedEncElt[count];
	arc->read((unsigned char*)elts, sizeof(LCondensedEncElt) * count);
	encoding->decompressMany(elts, item, count);
	delete [] elts;
}

#ifndef DEBUG_ENCODING
template <>
inline void Archive::write(REncodedElt& item) {
	RCondensedEncElt elt;
	encoding->compress(item, elt);
	arc->write((unsigned char*)&elt, sizeof(RCondensedEncElt));
}

template <>
inline void Archive::write(REncodedElt* item, int count) {
	RCondensedEncElt* elts = new RCondensedEncElt[count];
	encoding->compressMany(item, elts, count, false);
	arc->write((unsigned char*)elts, sizeof(RCondensedEncElt) * count);
	delete [] elts;
}

template <>
inline void Archive::read(REncodedElt& item) {
	RCondensedEncElt elt;
	arc->read((unsigned char*)&elt, sizeof(RCondensedEncElt));
	encoding->decompress(elt, item);
}

template <>
inline void Archive::read(REncodedElt* item, int count) {
	RCondensedEncElt* elts = new RCondensedEncElt[count];
	arc->read((unsigned char*)elts, sizeof(RCondensedEncElt) * count);
	encoding->decompressMany(elts, item, count);
	delete [] elts;
}
#endif // !DEBUG_ENCODING

#endif


