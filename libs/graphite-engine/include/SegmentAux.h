/*--------------------------------------------------------------------*//*:Ignore this sentence.
Copyright (C) 2005 SIL International. All rights reserved.

Distributable under the terms of either the Common Public License or the
GNU Lesser General Public License, as specified in the LICENSING.txt file.

File: SegmentAux.h
Responsibility: Sharon Correll
Last reviewed: Not yet.

Description:
	Auxiliary classes for the Segment class:
	- GlyphInfo
	- GlyphIterator
	- LayoutEnvironment
----------------------------------------------------------------------------------------------*/
#ifdef _MSC_VER
#pragma once
#endif
#ifndef SEGMENTAUX_INCLUDED
#define SEGMENTAUX_INCLUDED

//:End Ignore

namespace gr
{

class Segment;
class GrSlotOutput;
class IGrJustifier;
class GlyphInfo;

/*----------------------------------------------------------------------------------------------
	The GlyphIterator class allows the Graphite client to iterate over a list of glyphs
	for the segment, or a list of glyphs for a character.
----------------------------------------------------------------------------------------------*/
class GlyphIterator
: public std::iterator<std::random_access_iterator_tag, gr::GlyphInfo>
{
public:
	friend class GlyphInfo;
	friend class Segment;

	// Index--if there there is a non-contiguous list (m_pvislout) then this is an index
	// into that, (in which case it functions as an 'islout'). Otherwise it is simply an index 
	// into the glyph stream (ie, an 'iginf')
	size_t m_index;

	// Segment containing the glyphs being iterated over.
	const Segment *	m_pseg;

	// Sometimes, in the case of character-to-glyph look-ups or attached
	// children, we need to represent a non-contiguous list; in these cases
	// we first map through a vector of output-slot objects into the actual 
	// glyph-info store.
	const std::vector<int> * m_pvislout;

	// Default constructor--no output-slot mapping:
	GlyphIterator() throw (): m_index(0), m_pseg(NULL), m_pvislout(NULL) {}

protected:
	// Constructor with no output-slot mapping:
	GlyphIterator(Segment * const pseg, size_t iginf) throw()
	  : m_index(iginf), m_pseg(pseg), m_pvislout(NULL)
	{
		GrAssert(pseg);
		GrAssert(iginf <= seqSize());
	}

	// Constructor that includes output-slot mapping list, used for non-contiguous lists:
	GlyphIterator(Segment * const pseg, size_t islout, const std::vector<int> * pvislout)
	  : m_index(islout), m_pseg(pseg), m_pvislout(pvislout)
	{
		GrAssert(pseg);
		GrAssert(pvislout);
		GrAssert(islout <= pvislout->size());
		GrAssert(pvislout->size() <= seqSize());
	}

public:
	// Forward iterator requirements.
	reference	  operator*() const		{ return operator[](0); }
	pointer		  operator->() const		{ return &(operator*()); }
	GlyphIterator	& operator++() throw()		{ GrAssert(m_index < seqSize()); ++m_index; return *this; }
	GlyphIterator	  operator++(int) throw()	{ GlyphIterator tmp = *this; operator++(); return tmp; }

	// Bidirectional iterator requirements
	GlyphIterator	& operator--() throw()		{ GrAssert(0 < m_index); --m_index; return *this; }
	GlyphIterator	  operator--(int) throw()	{ GlyphIterator tmp = *this; operator--(); return tmp; }

	// Random access iterator requirements
	reference	  operator[](difference_type n) const;
	GlyphIterator	& operator+=(difference_type n)	throw()		{ m_index += n; GrAssert(m_index <= seqSize()); return *this; }
	GlyphIterator	  operator+(difference_type n) const throw()	{ GlyphIterator r = *this; return r += n; }
	GlyphIterator	& operator-=(difference_type n)	throw()		{ operator+=(-n); return *this; }
	GlyphIterator	  operator-(difference_type n) const throw()	{ GlyphIterator r = *this; return r += -n; }
 
	// Relational operators.
  	// Forward iterator requirements
	bool	operator==(const GlyphIterator & rhs) const throw()	{ GrAssert(isComparable(rhs)); return m_index == rhs.m_index; }
	bool	operator!=(const GlyphIterator & rhs) const throw()	{ return !(*this == rhs); }

	// Random access iterator requirements
	bool	operator<(const GlyphIterator & rhs) const throw()	{ GrAssert(isComparable(rhs)); return m_index < rhs.m_index; }
	bool	operator>(const GlyphIterator & rhs) const throw()	{ GrAssert(isComparable(rhs)); return m_index > rhs.m_index; }
	bool	operator<=(const GlyphIterator & rhs) const throw()	{ return !(*this > rhs); }
	bool	operator>=(const GlyphIterator & rhs) const throw()	{ return !(*this < rhs); }

	difference_type operator-(const GlyphIterator & rhs) const throw()	{ GrAssert(isComparable(rhs)); return m_index - rhs.m_index; }
 
private:
	bool isComparable(const GlyphIterator & rhs) const throw ()
	{
		return (m_pseg == rhs.m_pseg && m_pvislout == rhs.m_pvislout);
	}
	size_t seqSize() const throw();
};

inline GlyphIterator operator+(const GlyphIterator::difference_type n, const GlyphIterator & rhs)
{
	return rhs + n;
}


/*----------------------------------------------------------------------------------------------
	The GlyphInfo class provides access to details about a single glyph within a segment.
----------------------------------------------------------------------------------------------*/
class GlyphInfo		// hungarian: ginf
{
	friend class Segment;

public:
	// Default constructor:
	GlyphInfo()
	{
		m_pseg = NULL;
		m_pslout = NULL;
		m_islout = kInvalid;
	}

	gid16 glyphID();
	gid16 pseudoGlyphID();

	// Index of this glyph in the logical sequence; zero-based.
	size_t logicalIndex();

	// glyph's position relative to the left of the segment
	float origin();
	float advanceWidth();		// logical units
	float advanceHeight();	// logical units; zero for horizontal fonts
	float yOffset();
	gr::Rect bb();				// logical units
	bool isSpace();

	// first char associated with this glyph, relative to start of seg
	toffset firstChar();
	// last char associated with this glyph, relative to start of seg
	toffset lastChar();

	// Unicode bidi value
	unsigned int directionality();
	// Embedding depth
	unsigned int directionLevel();
	bool insertBefore();
	int	breakweight();

	bool isAttached() const throw();
	gr::GlyphIterator attachedClusterBase() const throw();
	float attachedClusterAdvance() const throw();
	std::pair<gr::GlyphIterator, gr::GlyphIterator> attachedClusterGlyphs() const;

	float maxStretch(size_t level);
	float maxShrink(size_t level);
	float stretchStep(size_t level);
	byte justWeight(size_t level);
	float justWidth(size_t level);
	float measureStartOfLine();
	float measureEndOfLine();

	size_t numberOfComponents();
	gr::Rect componentBox(size_t icomp);
	toffset componentFirstChar(size_t icomp);
	toffset componentLastChar(size_t icomp);

	bool erroneous();

protected:
	Segment * m_pseg;
	GrSlotOutput * m_pslout;
	size_t m_islout;
};


/*----------------------------------------------------------------------------------------------
	The GlyphInfo class provides access to details about a single glyph within a segment.
----------------------------------------------------------------------------------------------*/
class LayoutEnvironment
{
public:
	LayoutEnvironment()
	{
		// Defaults:
		m_fStartOfLine = true;
		m_fEndOfLine = true;
		m_lbBest = klbWordBreak;
		m_lbWorst = klbClipBreak;
		m_fRightToLeft = false;
		m_twsh = ktwshAll;
		m_pstrmLog = NULL;
		m_fDumbFallback = false;
		m_psegPrev = NULL;
		m_psegInit = NULL;
		m_pjust = NULL;
	}
	LayoutEnvironment(LayoutEnvironment & layout)
	{
		m_fStartOfLine = layout.m_fStartOfLine;
		m_fEndOfLine = layout.m_fEndOfLine;
		m_lbBest = layout.m_lbBest;
		m_lbWorst = layout.m_lbWorst;
		m_fRightToLeft = layout.m_fRightToLeft;
		m_twsh = layout.m_twsh;
		m_pstrmLog = layout.m_pstrmLog;
		m_fDumbFallback = layout.m_fDumbFallback;
		m_psegPrev = layout.m_psegPrev;
		m_psegInit = layout.m_psegInit;
		m_pjust = layout.m_pjust;
	}

	// Setters:
	inline void setStartOfLine(bool f)					{ m_fStartOfLine = f; }
	inline void setEndOfLine(bool f)					{ m_fEndOfLine = f; }
	inline void setBestBreak(LineBrk lb)				{ m_lbBest = lb; }
	inline void setWorstBreak(LineBrk lb)				{ m_lbWorst = lb; }
	inline void setRightToLeft(bool f)					{ m_fRightToLeft = f; }
	inline void setTrailingWs(TrWsHandling twsh)		{ m_twsh = twsh; }
	inline void setLoggingStream(std::ostream * pstrm)	{ m_pstrmLog = pstrm; }
	inline void setDumbFallback(bool f)					{ m_fDumbFallback = f; }
	inline void setPrevSegment(Segment * pseg)		{ m_psegPrev = pseg; }
	inline void setSegmentForInit(Segment * pseg)		{ m_psegInit = pseg; }
	inline void setJustifier(IGrJustifier * pjust)		{ m_pjust = pjust; }

	// Getters:
	inline bool startOfLine()				{ return m_fStartOfLine; }
	inline bool endOfLine()					{ return m_fEndOfLine; }
	inline LineBrk bestBreak()				{ return m_lbBest; }
	inline LineBrk worstBreak()				{ return m_lbWorst; }
	inline bool rightToLeft()				{ return m_fRightToLeft; }
	inline TrWsHandling trailingWs()		{ return m_twsh; }
	inline std::ostream * loggingStream()	{ return m_pstrmLog; }
	inline bool dumbFallback()				{ return m_fDumbFallback; }
	inline Segment * prevSegment()			{ return m_psegPrev; }
	inline Segment * segmentForInit()		{ return m_psegInit; }
	inline IGrJustifier * justifier()		{ return m_pjust; }

protected:
	bool m_fStartOfLine;
	bool m_fEndOfLine;
	LineBrk m_lbBest;
	LineBrk m_lbWorst;
	bool m_fRightToLeft;
	TrWsHandling m_twsh;
	std::ostream * m_pstrmLog;
	bool m_fDumbFallback;
	Segment * m_psegPrev;
	Segment * m_psegInit;
	IGrJustifier * m_pjust;
};

} // namespace gr

#if defined(GR_NO_NAMESPACE)
using namespace gr;
#endif

#endif  // !SEGMENTAUX_INCLUDED
