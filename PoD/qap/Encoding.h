#pragma once

#include "Types.h"
#include "Field.h"


template <typename EncodedEltType>
struct MultiExpPrecompTable {
	int numBits;  // Number of bits to take from each exponent
	int numTerms; // Number of bases to combine into a table.  Currently, code can only handle values of 1 and 2.
	int len;			// Number of bases this table covers
	int numTables;
	long long sizeInMB; // Memory used
	multi_exponent_t* multiExp;	// Used for the old style of multi-exp
	EncodedEltType** precompTables;
} ;

#ifndef DEBUG_ENCODING
//typedef digit_t LEncodedElt[2*4];		// Two field elements to define an affine point on the base curve
//typedef digit_t REncodedElt[2*8];		// Four field elements to define an affine point on the twist curve
typedef digit_t LEncodedElt[3*4];		// Three field elements to define a projective point on the base curve
typedef digit_t REncodedElt[3*8];		// Six field elements to define a projective point on the twist curve
typedef digit_t EncodedProduct[48];	

typedef digit_t LEncodedEltAff[2*4];		// Two field elements to define an affinized point on the base curve
typedef digit_t REncodedEltAff[2*8];		// Four field elements to define a affinized point on the twist curve

typedef digit_t LCondensedEncElt[4];		// One field element to define a compressed point on the base curve
typedef digit_t RCondensedEncElt[8];		// One field element to define a compressed point on the twist curve

#ifdef __FFLIB
class __declspec(dllexport) Encoding {
#else
class Encoding {
#endif
public:
	Encoding();
	~Encoding();

	virtual void encode(FieldElt& in, LEncodedElt& out);	
	virtual void encode(FieldElt& in, REncodedElt& out);

	virtual void copy(LEncodedElt& src, LEncodedElt& dst);
	virtual void copy(REncodedElt& src, REncodedElt& dst);

	// Multiplies the value encoded in g by the constant c.  Result goes in r.
	virtual void mul(LEncodedElt& g, FieldElt& c, LEncodedElt& r);
	virtual void mul(REncodedElt& g, FieldElt& c, REncodedElt& r);

	virtual void mul(LEncodedElt &a, REncodedElt &b, EncodedProduct& r);
	virtual void add(EncodedProduct& a, EncodedProduct& b, EncodedProduct& r);
	virtual void sub(EncodedProduct& a, EncodedProduct& b, EncodedProduct& r);
	virtual bool equals(EncodedProduct& a, EncodedProduct& b);

	// Computes r <- g_{a_i}^{b_i} for 0 <= i < len.  Uses the Enigma version.
	virtual void multiMulOld(multi_exponent_t* multiExp, int len, LEncodedElt& r);
	virtual void multiMulOld(multi_exponent_t* multiExp, int len, REncodedElt& r);

	// Wrapper for the version below, if you already have an array, this one will build the indirection layer for you
	template <typename EncodedEltType>
	void precomputeMultiPowers(MultiExpPrecompTable<EncodedEltType>& table, EncodedEltType* bases, int len, int numUses, int maxMemGb, bool precompFree);

	// numUses = number of times we expect to use this table
	template <typename EncodedEltType>
	void precomputeMultiPowers(MultiExpPrecompTable<EncodedEltType>& table, EncodedEltType** bases, int len, int numUses, int maxMemGb, bool precompFree);
	
	template <typename EncodedEltType>
	void deleteMultiTables(MultiExpPrecompTable<EncodedEltType>& precompTables);

	// Wrapper for the version below, if you already have an array, this one will build the indirection layer for you
	template <typename EncodedEltType>
	void multiMul(MultiExpPrecompTable<EncodedEltType>& precompTables, FieldElt* exp, int len, EncodedEltType& r);

	// Computes r <- g_{a_i}^{b_i} for 0 <= i < len
	template <typename EncodedEltType>
	void multiMul(MultiExpPrecompTable<EncodedEltType>& precompTables, FieldElt** exp, int len, EncodedEltType& r);

	template <typename EncodedEltType>
	void multiMulVert(MultiExpPrecompTable<EncodedEltType>& precompTables, FieldElt** exp, int len, EncodedEltType& r);
	

	// Adds two encoded elements.  Result goes in r.
	virtual void add(LEncodedElt& a, LEncodedElt& b, LEncodedElt& r);
	virtual void add(REncodedElt& a, REncodedElt& b, REncodedElt& r);

	// Subtracts two encoded elements.  Result goes in r.
	virtual void sub(LEncodedElt& a, LEncodedElt& b, LEncodedElt& r);
	virtual void sub(REncodedElt& a, REncodedElt& b, REncodedElt& r);
	
	// Special case of doubling is more efficient
	virtual void doubleIt(LEncodedElt& a, LEncodedElt& b);
	virtual void doubleIt(REncodedElt& a, REncodedElt& b);

	virtual bool equals(LEncodedElt& a, LEncodedElt& b);
	virtual bool equals(REncodedElt& a, REncodedElt& b);
	
	virtual void affinize(LEncodedElt& in, LEncodedElt& out);
	virtual void affinize(REncodedElt& in, REncodedElt& out);
	virtual void affinize(LEncodedElt& in, LEncodedEltAff& out);
	virtual void affinize(REncodedElt& in, REncodedEltAff& out);
	virtual void affinizeMany(LEncodedElt* in, LEncodedEltAff* out, int count, bool overWriteOk);	// overWriteOk = true if okay to overwrite in
	virtual void affinizeMany(REncodedElt* in, REncodedEltAff* out, int count, bool overWriteOk);	// overWriteOk = true if okay to overwrite in
	virtual void projectivize(LEncodedEltAff& in, LEncodedElt& out);
	virtual void projectivize(REncodedEltAff& in, REncodedElt& out);
	virtual void projectivizeMany(LEncodedEltAff* in, LEncodedElt* out, int count);
	virtual void projectivizeMany(REncodedEltAff* in, REncodedElt* out, int count);

	virtual void compress(LEncodedElt& in, LCondensedEncElt& out);
	virtual void compress(REncodedElt& in, RCondensedEncElt& out);
	virtual void compress(LEncodedEltAff& in, LCondensedEncElt& out);
	virtual void compress(REncodedEltAff& in, RCondensedEncElt& out);
	virtual void compressMany(LEncodedElt* in, LCondensedEncElt* out, int count, bool overWriteOk);	// overWriteOk = true if okay to overwrite in
	virtual void compressMany(REncodedElt* in, RCondensedEncElt* out, int count, bool overWriteOk);	// overWriteOk = true if okay to overwrite in
	virtual void decompress(LCondensedEncElt& in, LEncodedElt& out);
	virtual void decompress(RCondensedEncElt& in, REncodedElt& out);
	virtual void decompressMany(LCondensedEncElt* in, LEncodedElt* out, int count);
	virtual void decompressMany(RCondensedEncElt* in, REncodedElt* out, int count);

	void pair(LEncodedElt& L, REncodedElt& R, EncodedProduct& result);
	void pairAffine(LEncodedEltAff& L, REncodedEltAff& R, EncodedProduct& result);

	// Checks whether the plaintexts inside the encodings obey:
	// L1*R1 - L2*R2 == L3*R3
	virtual bool mulSubEquals(LEncodedElt& L1, REncodedElt& R1, LEncodedElt& L2, REncodedElt& R2, LEncodedElt& L3, REncodedElt& R3);

	// Checks whether the plaintexts inside the encodings obey:
	// L1*R1 == L2*R2
	virtual bool mulEquals(LEncodedElt& L1, REncodedElt& R1, LEncodedElt& L2, REncodedElt& R2);

	Field* getSrcField() { return srcField; }	// Encoding takes elements from srcField to dstField

	virtual void zero(LEncodedElt& a);
	virtual void zero(REncodedElt& a);

	virtual void one(LEncodedElt& a);
	virtual void one(REncodedElt& a);

	virtual bool isZero(LEncodedElt& elt);
	virtual bool isZero(REncodedElt& elt);
	
	virtual bool isZeroAff(LEncodedEltAff& elt);
	virtual bool isZeroAff(REncodedEltAff& elt);

	virtual bool isZero(LCondensedEncElt& elt);
	virtual bool isZero(RCondensedEncElt& elt);

	virtual void testOnCurve(LEncodedElt& elt);
	virtual void testOnCurve(REncodedElt& elt);
	virtual void testOnCurve(LEncodedEltAff& elt);
	virtual void testOnCurve(REncodedEltAff& elt);

	virtual void print(LEncodedElt& elt);
	virtual void print(REncodedElt& elt);

	// Max memory to use.  If precompFree, then fill maxMem.  Otherwise, assume we throw everything away, so precomp needs to pay for itself in one instance
	void allocatePreCalcMem(long long numExps, int maxMemGb, bool precompFree); // Helper for precomputePowersFast
	virtual void precomputePowersFast(long long numExps, int maxMemGb, bool precompFree);
	virtual void precomputePowersSlow();
	virtual void deletePrecomputePowers();
	virtual void encodeSlow(FieldElt& in, LEncodedElt& out);
	virtual void encodeSlow(FieldElt& in, REncodedElt& out);

	virtual void encodeSlowest(FieldElt& in, LEncodedElt& out);
	virtual void encodeSlowest(FieldElt& in, REncodedElt& out);
#ifdef PIPPENGER
	virtual void encode2(FieldElt& in, LEncodedElt& out);
	virtual void precomputePowers2();
#endif

	void resetCount();
	void printCount();

	void print_stats();
	void reset_stats();

//protected:

	int computeIndex(FieldElt& in, int j, int k);

	pairing_friendly_curve_t bn;  
	bigctx_t BignumCtx;
	bigctx_t* pbigctx;
	mp_modulus_t* p;
	field_desc_t* msExponentField;
	Field* srcField;

	digit_t arithTempSpace[260];

	// Projective versions of the Base and Twist generators (also a version of g^1)
	LEncodedElt lGen;
	REncodedElt rGen;

	// Used for fast encoding calculations
	int h, v, a, b;
	LEncodedElt** lG;
	REncodedElt** rG;

	// Frequently encoded elts
	LEncodedElt lZero;
	REncodedElt rZero;

	int numMults;
	int numSquares;

	int num_L_addsub;
  int num_L_double;
  int num_L_constmul;

	int num_R_addsub;
  int num_R_double;
  int num_R_constmul;
  int num_pair;

};

#else	// #define DEBUG_ENCODING

typedef FieldElt EncodedElt;
typedef EncodedElt LEncodedElt;		
typedef EncodedElt REncodedElt;		
typedef EncodedElt EncodedProduct;
typedef EncodedElt LCondensedEncElt; 
typedef EncodedElt RCondensedEncElt;	

class Encoding {
public:
	Encoding();
	~Encoding();

	virtual void encode(FieldElt& in, EncodedElt& out);
	virtual void encodeSlow(FieldElt& in, LEncodedElt& out) { encode(in, out); }
	virtual void encodeSlowest(FieldElt& in, LEncodedElt& out) { encode(in, out); }

	virtual void copy(EncodedElt& src, EncodedElt& dst);

	// Multiplies the value encoded in g by the constant c.  Result goes in r.
	virtual void mul(EncodedElt& g, FieldElt& c, EncodedElt& r);

	// Adds two encoded elements.  Result goes in r.
	virtual void add(EncodedElt& a, EncodedElt& b, EncodedElt& r);

	// Subtracts two encoded elements.  Result goes in r.
	virtual void sub(EncodedElt& a, EncodedElt& b, EncodedElt& r);

	virtual bool equals(EncodedElt& a, EncodedElt& b);

	void precomputePowersFast(long long numExps, int maxMemGb, bool precompFree);
	virtual void precomputePowersSlow() { ; }
	virtual void deletePrecomputePowers() {;}

	template <typename EncodedEltType>
	void precomputeMultiPowers(MultiExpPrecompTable<EncodedEltType>& table, EncodedEltType* bases, int len, int numUses, int maxMemGb, bool precompFree);	
	template <typename EncodedEltType>
	void precomputeMultiPowers(MultiExpPrecompTable<EncodedEltType>& table, EncodedEltType** bases, int len, int numUses, int maxMemGb, bool precompFree);	

	template <typename EncodedEltType>
	void deleteMultiTables(MultiExpPrecompTable<EncodedEltType>& table);

	// Computes r <- g_{a_i}^{b_i} for 0 <= i < len
	void multiMulOld(multi_exponent_t* multiExp, int len, LEncodedElt& r);
	template <typename EncodedEltType>
	void multiMulHoriz(MultiExpPrecompTable<EncodedEltType>& table, FieldElt* exp, int len, EncodedEltType& r);
	template <typename EncodedEltType>
	void multiMul(MultiExpPrecompTable<EncodedEltType>& table, FieldElt** exp, int len, EncodedEltType& r);
	template <typename EncodedEltType>
	void multiMul(MultiExpPrecompTable<EncodedEltType>& table, FieldElt* exp, int len, EncodedEltType& r);

	// Checks whether the plaintexts inside the encodings obey:
	// L1*R1 - L2*R2 == L3*R3
	virtual bool mulSubEquals(LEncodedElt& L1, REncodedElt& R1, LEncodedElt& L2, REncodedElt& R2, LEncodedElt& L3, REncodedElt& R3);

	// Checks whether the plaintexts inside the encodings obey:
	// L1*R1 == L2*R2
	virtual bool mulEquals(LEncodedElt& L1, REncodedElt& R1, LEncodedElt& L2, REncodedElt& R2);

	Field* getSrcField() { return srcField; }	// Encoding takes elements from srcField to dstField

	virtual void zero(EncodedElt& a);
	virtual void one(EncodedElt& a);

	virtual void affinize(EncodedElt& in, EncodedElt& out) { copy(in, out); }
	virtual void affinizeMany(EncodedElt* in, LCondensedEncElt* out, int count, bool overWriteOk) { for (int i = 0; i < count; i++) { copy (in[i], out[i]); } }
	virtual void projectivize(EncodedElt& in, EncodedElt& out) { copy(in, out); }
	virtual void projectivizeMany(EncodedElt* in, EncodedElt* out, int count) { for (int i = 0; i < count; i++) { copy (in[i], out[i]); } }

	virtual void compress(LEncodedElt& in, LCondensedEncElt& out);
	virtual void compressMany(LEncodedElt* in, LCondensedEncElt* out, int count, bool overWriteOk);	// overWriteOk = true if okay to overwrite in
	virtual void decompress(LCondensedEncElt& in, LEncodedElt& out);
	virtual void decompressMany(LCondensedEncElt* in, LEncodedElt* out, int count);

	virtual void doubleIt(EncodedElt& a, EncodedElt& b);

	virtual void print(EncodedElt& elt);

	void resetCount();
	void printCount();

	void print_stats() {}
	void reset_stats() {}

	mp_modulus_t* p;
	
	Field* srcField;
protected:
	bigctx_t BignumCtx;
	bigctx_t* pbigctx;
	
	field_desc_t* msExponentField;

	EncodedElt Zero;
	EncodedElt One;

	int num_exponent_bits;

	int numMults;
	int numSquares;
};
#endif	// DEBUG_ENCODING

