/*--------------------------------------------------------------------*//*:Ignore this sentence.
Copyright (C) 1999, 2001 SIL International. All rights reserved.

Distributable under the terms of either the Common Public License or the
GNU Lesser General Public License, as specified in the LICENSING.txt file.

File: GrFeatureValues.h
Responsibility: Sharon Correll
Last reviewed: Not yet.

Description:
	

----------------------------------------------------------------------------------------------*/
#ifdef _MSC_VER
#pragma once
#endif
#ifndef GR_FEATVAL_INCLUDED
#define GR_FEATVAL_INCLUDED

//:End Ignore

namespace gr
{

/*----------------------------------------------------------------------------------------------
	A convenient way to group together style and feature information that interacts
	with the rules.
	
	Hungarian: fval
----------------------------------------------------------------------------------------------*/
class GrFeatureValues
{
	friend class GrCharStream;
	friend class GrSlotAbstract;
	friend class GrSlotState;

public:
	//	Standard constructor:
	GrFeatureValues()
	{
		m_nStyleIndex = 0;
		memset(m_rgnFValues, 0, (kMaxFeatures * isizeof(int)));
	}

	//	Copy constructor:
	GrFeatureValues(const GrFeatureValues & fval)
	{
		m_nStyleIndex = fval.m_nStyleIndex;
		memcpy(m_rgnFValues, fval.m_rgnFValues, (kMaxFeatures * isizeof(int)));
	}

	//	For transduction logging:
#ifdef TRACING
	void WriteXductnLog(GrTableManager * ptman, std::ostream &);
#endif // TRACING

protected:
	int		m_nStyleIndex;
	int		m_rgnFValues[kMaxFeatures];
};

} // namespace gr


#endif // !GR_FEATVAL_INCLUDED
