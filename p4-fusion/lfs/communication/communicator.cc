#include "communicator.h"
#include "lfscomm.h"
#include "s3comm.h"
#include "openssl/sha.h"

std::unique_ptr<Communicator> Communicator::CreateLFS(const std::string& serverURL, const std::string& username, const std::string& password)
{
	return std::unique_ptr<LFSComm>(new LFSComm(serverURL, username, password));
}

std::unique_ptr<Communicator> Communicator::CreateS3(const std::string& serverURL, const std::string& bucket, const std::string& repository, const std::string& username, const std::string& password)
{
	return std::unique_ptr<S3Comm>(new S3Comm(serverURL, bucket, repository, username, password));
}
