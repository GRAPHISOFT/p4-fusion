#include "s3comm.h"
#include "lfs/lfs_client.h"
#include <aws/core/Aws.h>
#include <aws/core/Region.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/PutObjectRequest.h>
#include <aws/core/auth/AWSCredentials.h>
#include <aws/s3/S3ClientConfiguration.h>
#include <aws/core/utils/stream/PreallocatedStreamBuf.h>

S3Comm::S3Comm(const std::string& serverURL, const std::string& bucket, const std::string& repository, const std::string& username, const std::string& password)
	: m_ServerURL(serverURL)
	, m_Bucket(bucket)
	, m_Repository(repository)
	, m_Username(username)
	, m_Password(password)
{
}

Communicator::UploadResult S3Comm::UploadFile(const std::vector<char>& fileContents) const
{
	std::string oid = LFSClient::CalcOID(fileContents);

	Aws::Auth::AWSCredentials credentials(m_Username, m_Password);

	bool isHttps = m_ServerURL.find("https://") == 0;

    Aws::S3::S3ClientConfiguration config;
    config.endpointOverride = m_ServerURL;
    config.scheme = isHttps ? Aws::Http::Scheme::HTTPS : Aws::Http::Scheme::HTTP;
    config.verifySSL = isHttps;
    config.region = Aws::Region::US_EAST_1; // MinIO ignores region, but AWS SDK requires it
	config.useVirtualAddressing = false;

	Aws::S3::S3Client s3_client(credentials, config,
        Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::Never, false);

	Aws::S3::Model::PutObjectRequest object_request;
	object_request.SetBucket(m_Bucket);
	object_request.SetKey(m_Repository + "/" + oid);

	auto streamBuf = Aws::MakeShared<Aws::Utils::Stream::PreallocatedStreamBuf>("",
		reinterpret_cast<unsigned char*>(const_cast<char*>(fileContents.data())), 
		fileContents.size());
	auto stream = Aws::MakeShared<Aws::IOStream>("", streamBuf.get());
    stream->seekg(0, std::ios::beg);

    object_request.SetBody(stream);

	auto upload_result = s3_client.PutObject(object_request);
	return upload_result.IsSuccess() ?
		UploadResult::Uploaded :
		UploadResult::Error;
}
