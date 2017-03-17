/* -------------------------------------------------------------------------
//	File Name	:	ErrorCheck.h
//	Author		:	Zhang Fan
//	Create Time	:	2012-3-19 16:49:55
//	Description	:   error check and code path control
//
// -----------------------------------------------------------------------*/

#ifndef __ERRORCHECK_H__
#define __ERRORCHECK_H__

// #define DISABLE_ASSERT

//---------------------------------------------------------------------------
#ifdef _MSC_VER
#define __X_FUNCTION__ __FUNCTION__
#else
#define __X_FUNCTION__ __PRETTY_FUNCTION__
#endif

// -------------------------------------------------------------------------

#define CHECK(exp)														\
    do {																	\
        if (!(exp))															\
        {																	\
            goto Exit0;														\
        }																	\
    } while(0)

#define ERROR_CHECK(exp)												\
    do {																	\
    if (!(exp))															    \
    {																	\
        std::cerr << "ERROR_CHECK_BOOL:" #exp <<  __FILE__ << ":(" << __LINE__ <<")" << std::endl;                   \
        goto Exit0;														    \
    }																	\
    } while(0)

#define CHECKEX(exp, exp1)												\
    do {																	\
    if (!(exp))															    \
        {																	\
        exp1;															    \
        goto Exit0;														    \
        }																	\
    } while(0)

#define ERROR_CHECKEX(exp, exp1)										\
    do {																	\
    if (!(exp))			    												\
    {																	\
        std::cerr << "ERROR_CHECK_BOOLEX:" #exp <<  __FILE__ << ":(" << __LINE__ <<")" << std::endl;                   \
        exp1;															    \
        goto Exit0;														    \
        }																	\
    } while(0)

#define QUIT()          \
    do                  \
    {                   \
    goto Exit0;         \
    } while (0)

//--------------------------------------------------------------------------
#endif /* __ERRORCHECK_H__ */