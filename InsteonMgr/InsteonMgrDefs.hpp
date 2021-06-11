//
//  InsteonMgrDefs.h
//  plmtest
//
//  Created by Vincent Moscaritolo on 3/3/21.
//

#ifndef InsteonMgrDefs_h
#define InsteonMgrDefs_h

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <functional>

#define DO_PRAGMA(X) _Pragma(#X)
#define DISABLE_WARNING(warningName) \
	 DO_PRAGMA(GCC diagnostic ignored #warningName)
#define DISABLE_WARNING_POP           \
	DO_PRAGMA(GCC diagnostic pop)



typedef std::function<void(bool didSucceed)> boolCallback_t;

typedef  unsigned long eTag_t;
 

#endif /* InsteonMgrDefs_h */
