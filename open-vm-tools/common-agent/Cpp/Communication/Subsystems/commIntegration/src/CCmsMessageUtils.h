/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CCmsMessageUtil_h_
#define CCmsMessageUtil_h_

#include <openssl/cms.h>

#include "Memory/DynamicArray/DynamicArrayInc.h"
#include <openssl/conf.h>
#include <openssl/engine.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/ssl.h>

namespace Caf {

class CCmsMessageUtils {
public: // Convert
	static BIO* inputBufferToBio(
			const SmartPtrCDynamicByteArray& inputBuffer);

	static std::deque<BIO*> inputFilesToBio(
			const Cdeqstr& inputFileCollection);

	static BIO* inputFileToBio(
			const std::string& inputFile);

	static BIO* inputToBio(
			const SmartPtrCDynamicByteArray& inputBuffer,
			const std::string& inputPath);

	static BIO* outputPathToBio(
			const std::string& outputPath);

	static BIO* outputToBio(
			const SmartPtrCDynamicByteArray& outputBuffer,
			const std::string& outputPath);

	static void bioToOutput(
			BIO* bio,
			SmartPtrCDynamicByteArray& outputBuffer,
			const std::string& outputPath);

	static void bioToOutputBuffer(
			BIO* bio,
			SmartPtrCDynamicByteArray& outputBuffer);

	static void bioToOutputFile(
			BIO* bio,
			const std::string& outputPath);

	static std::deque<X509*> biosToX509(
			std::deque<BIO*> bioCollection);

	static X509* bioToX509(
			BIO* bio);

	static EVP_PKEY* bioToPrivateKey(
			BIO* bio);

	static const SSL_METHOD* protocolToSslMethod(
			const std::string& protocol);

public: // Create
	static BIO* createWriteBio();

	static STACK_OF(X509)* createX509Stack(
			X509* x509,
			X509* x5091 = NULL,
			X509* x5092 = NULL);

	static X509_STORE* createX509Store(
			std::deque<X509*> x509Collection);

public: // Free
	static void free(
			CMS_ContentInfo* contentInfo);

	static void free(
			X509* x509);

	static void free(
			std::deque<X509*> x509Collection);

	static void free(
			STACK_OF(X509)* x509Stack);

	static void free(
			BIO* bio);

	static void free(
			std::deque<BIO*> bioCollection);

	static void free(
			X509_STORE* x509Store);

	static void free(
			EVP_PKEY* privateKey);

public: // Log
	static void logSslErrors();

	static void logCiphers(
			const std::string& prefix,
			const SSL* ssl);

private:
	CAF_CM_DECLARE_NOCREATE(CCmsMessageUtils);
};

}

#endif // #ifndef CCmsMessageUtil_h_
