//
//  InsteonAPISecretMgr.hpp
//  InsteonServer
//
//  Created by Vincent Moscaritolo on 6/7/21.
//

#ifndef InsteonAPISecretMgr_hpp
#define InsteonAPISecretMgr_hpp

#include <stdio.h>

#include "ServerCmdQueue.hpp"
#include "InsteonDB.hpp"


class InsteonAPISecretMgr : public APISecretMgr {

public:
	InsteonAPISecretMgr(InsteonDB* db);
	
	virtual bool apiSecretCreate(string APIkey, string APISecret);
	virtual bool apiSecretDelete(string APIkey);
	virtual bool apiSecretGetSecret(string APIkey, string &APISecret);
	
private:
	InsteonDB* 	 		_db;

};


#endif /* InsteonAPISecretMgr_hpp */
