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



typedef std::function<void(bool didSucceed)> boolCallback_t;

typedef  unsigned long eTag_t;
 

#endif /* InsteonMgrDefs_h */
