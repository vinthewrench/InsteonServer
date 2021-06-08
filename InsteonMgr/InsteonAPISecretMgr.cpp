//
//  InsteonAPISecretMgr.cpp
//  InsteonServer
//
//  Created by Vincent Moscaritolo on 6/7/21.
//

#include "InsteonAPISecretMgr.hpp"
#include "LogMgr.hpp"

InsteonAPISecretMgr::InsteonAPISecretMgr(InsteonDB* db){
	_db = db;
}

bool InsteonAPISecretMgr::apiSecretCreate(string APIkey, string APISecret){
	return _db->apiSecretCreate(APIkey,APISecret );
}

bool InsteonAPISecretMgr::apiSecretDelete(string APIkey){
	return _db->apiSecretDelete(APIkey);
}

bool InsteonAPISecretMgr::apiSecretGetSecret(string APIkey, string &APISecret){
	return _db->apiSecretGetSecret(APIkey, APISecret);
}
