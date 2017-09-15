/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Memory/DynamicArray/DynamicArrayInc.h"
#include "Exception/CCafException.h"
#include "CCmsMessageUtils.h"

using namespace Caf;

BIO* CCmsMessageUtils::inputBufferToBio(
		const SmartPtrCDynamicByteArray& inputBuffer) {
	CAF_CM_STATIC_FUNC("CCmsMessageUtils", "inputBufferToBio");
	CAF_CM_VALIDATE_SMARTPTR(inputBuffer);

	BIO* rc = BIO_new_mem_buf(
			inputBuffer->getNonConstPtr(), static_cast<int32>(inputBuffer->getByteCount()));
	if (! rc) {
		logSslErrors();
		CAF_CM_EXCEPTION_VA0(E_FAIL, "BIO_new_mem_buf Failed");
	}

	return rc;
}

std::deque<BIO*> CCmsMessageUtils::inputFilesToBio(
		const Cdeqstr& inputFileCollection) {
	CAF_CM_STATIC_FUNC_VALIDATE("CCmsMessageUtils", "inputFilesToBio");
	CAF_CM_VALIDATE_STL(inputFileCollection);

	std::deque<BIO*> rc;
	for (TConstIterator<Cdeqstr> elemIter(inputFileCollection); elemIter; elemIter++) {
		const std::string elem = *elemIter;
		rc.push_back(inputFileToBio(elem));
	}

	return rc;
}

BIO* CCmsMessageUtils::inputFileToBio(
		const std::string& inputFile) {
	CAF_CM_STATIC_FUNC("CCmsMessageUtils", "inputFileToBio");
	CAF_CM_VALIDATE_STRING(inputFile);

	BIO* rc = BIO_new_file(inputFile.c_str(), "r");
	if (! rc) {
		logSslErrors();
		CAF_CM_EXCEPTION_VA1(E_FAIL,
				"BIO_new_file Failed - %s", inputFile.c_str());
	}

	return rc;
}

BIO* CCmsMessageUtils::inputToBio(
		const SmartPtrCDynamicByteArray& inputBuffer,
		const std::string& inputPath) {
	CAF_CM_STATIC_FUNC("CCmsMessageUtils", "inputToBio");

	BIO* rc = NULL;
	if (! inputBuffer.IsNull()) {
		rc = inputBufferToBio(inputBuffer);
	} else if (! inputPath.empty()) {
		rc = inputFileToBio(inputPath);
	} else {
		CAF_CM_EXCEPTION_VA0(E_FAIL, "Must provide buffer or filename");
	}

	return rc;
}

BIO* CCmsMessageUtils::outputPathToBio(
		const std::string& outputPath) {
	CAF_CM_STATIC_FUNC("CCmsMessageUtils", "outputPathToBio");
	CAF_CM_VALIDATE_STRING(outputPath);

	BIO* rc = BIO_new_file(outputPath.c_str(), "w");
	if (! rc) {
		logSslErrors();
		CAF_CM_EXCEPTION_VA1(E_FAIL,
				"BIO_new_file Failed - %s", outputPath.c_str());
	}

	return rc;
}

BIO* CCmsMessageUtils::outputToBio(
		const SmartPtrCDynamicByteArray& outputBuffer,
		const std::string& outputPath) {
	CAF_CM_STATIC_FUNC("CCmsMessageUtils", "outputToBio");

	BIO* rc = NULL;
	if (! outputBuffer.IsNull()) {
		rc = createWriteBio();
	} else if (! outputPath.empty()) {
		rc = outputPathToBio(outputPath);
	} else {
		CAF_CM_EXCEPTION_VA0(E_FAIL, "Must provide buffer or filename");
	}

	return rc;
}

void CCmsMessageUtils::bioToOutput(
		BIO* bio,
		SmartPtrCDynamicByteArray& outputBuffer,
		const std::string& outputPath) {
	CAF_CM_STATIC_FUNC("CCmsMessageUtils", "bioToOutput");
	CAF_CM_VALIDATE_PTR(bio);

	if (! outputBuffer.IsNull()) {
		bioToOutputBuffer(bio, outputBuffer);
	} else if (! outputPath.empty()) {
		bioToOutputFile(bio, outputPath);
	} else {
		CAF_CM_EXCEPTION_VA0(E_FAIL, "Must provide buffer or filename");
	}
}

void CCmsMessageUtils::bioToOutputBuffer(
		BIO* bio,
		SmartPtrCDynamicByteArray& outputBuffer) {
	CAF_CM_STATIC_FUNC_VALIDATE("CCmsMessageUtils", "bioToOutputBuffer");
	CAF_CM_VALIDATE_PTR(bio);

	BUF_MEM* bptr = NULL;
	BIO_get_mem_ptr(bio, &bptr);

	// Casting to void to ignore return value and eliminate compiler warning
	(void) BIO_set_close(bio, BIO_NOCLOSE); /* So BIO_free() leaves BUF_MEM alone */

	outputBuffer.CreateInstance();
	outputBuffer->allocateBytes(bptr->length);
	outputBuffer->memCpy(bptr->data, bptr->length);
}

void CCmsMessageUtils::bioToOutputFile(
		BIO* bio,
		const std::string& outputPath) {
	CAF_CM_STATIC_FUNC_VALIDATE("CCmsMessageUtils", "bioToOutputFile");
	CAF_CM_VALIDATE_PTR(bio);
	CAF_CM_VALIDATE_STRING(outputPath);
}

std::deque<X509*> CCmsMessageUtils::biosToX509(
		std::deque<BIO*> bioCollection) {
	CAF_CM_STATIC_FUNC_VALIDATE("CCmsMessageUtils", "biosToX509");
	CAF_CM_VALIDATE_STL(bioCollection);

	std::deque<X509*> rc;
	for (TConstIterator<std::deque<BIO*> > elemIter(bioCollection); elemIter; elemIter++) {
		BIO* elem = *elemIter;
		rc.push_back(bioToX509(elem));
	}

	return rc;
}

X509* CCmsMessageUtils::bioToX509(
		BIO* bio) {
	CAF_CM_STATIC_FUNC("CCmsMessageUtils", "bioToX509");
	CAF_CM_VALIDATE_PTR(bio);

	X509* rc = PEM_read_bio_X509(bio, NULL, 0, NULL);
	if (! rc) {
		logSslErrors();
		CAF_CM_EXCEPTION_VA0(E_FAIL, "PEM_read_bio_X509 Failed");
	}

	return rc;
}

EVP_PKEY* CCmsMessageUtils::bioToPrivateKey(
		BIO* bio) {
	CAF_CM_STATIC_FUNC("CCmsMessageUtils", "bioToPrivateKey");
	CAF_CM_VALIDATE_PTR(bio);

	EVP_PKEY* rc = PEM_read_bio_PrivateKey(bio, NULL, 0, NULL);
	if (! rc) {
		logSslErrors();
		CAF_CM_EXCEPTION_VA0(E_FAIL, "PEM_read_bio_PrivateKey Failed");
	}

	return rc;
}

const SSL_METHOD* CCmsMessageUtils::protocolToSslMethod(
		const std::string& protocol) {
	CAF_CM_STATIC_FUNC("CCmsMessageUtils", "protocolToSslMethod");
	CAF_CM_VALIDATE_STRING(protocol);

	const SSL_METHOD* rc;
	if (protocol.compare("TLSv1_2") == 0) {
		rc = TLSv1_2_method();
	} else {
		CAF_CM_EXCEPTION_VA1(E_FAIL,
				"Unknown protocol - %s", protocol.c_str());
	}

	return rc;
}

BIO* CCmsMessageUtils::createWriteBio() {
	CAF_CM_STATIC_FUNC("CCmsMessageUtils", "createWriteBio");

	BIO* rc = BIO_new(BIO_s_mem());
	if (! rc) {
		logSslErrors();
		CAF_CM_EXCEPTION_VA0(E_FAIL, "BIO_new Failed");
	}

	return rc;
}

STACK_OF(X509)* CCmsMessageUtils::createX509Stack(
		X509* x509,
		X509* x5091,
		X509* x5092) {
	CAF_CM_STATIC_FUNC("CCmsMessageUtils", "createX509Stack");
	CAF_CM_VALIDATE_PTR(x509);

	/* Create recipient STACK and add recipient cert to it */
	STACK_OF(X509)* rc = sk_X509_new_null();
	if (! rc) {
		logSslErrors();
		CAF_CM_EXCEPTION_VA0(E_FAIL, "sk_X509_new_nuss Failed");
	}

	if (!sk_X509_push(rc, x509)) {
		logSslErrors();
		CAF_CM_EXCEPTION_VA0(E_FAIL, "sk_X509_push Failed");
	}

	if (x5091) {
		if (!sk_X509_push(rc, x5091)) {
			logSslErrors();
			CAF_CM_EXCEPTION_VA0(E_FAIL, "sk_X509_push Failed");
		}
	}

	if (x5092) {
		if (!sk_X509_push(rc, x5092)) {
			logSslErrors();
			CAF_CM_EXCEPTION_VA0(E_FAIL, "sk_X509_push Failed");
		}
	}

	return rc;
}

X509_STORE* CCmsMessageUtils::createX509Store(
		std::deque<X509*> x509Collection) {
	CAF_CM_STATIC_FUNC("CCmsMessageUtils", "createX509Store");
	CAF_CM_VALIDATE_STL(x509Collection);

	X509_STORE* rc = X509_STORE_new();
	if (! rc) {
		logSslErrors();
		CAF_CM_EXCEPTION_VA0(E_FAIL, "X509_STORE_new Failed");
	}

	for (TConstIterator<std::deque<X509*> > elemIter(x509Collection); elemIter; elemIter++) {
		X509* elem = *elemIter;
		if (!X509_STORE_add_cert(rc, elem)) {
			logSslErrors();
			CAF_CM_EXCEPTION_VA0(E_FAIL, "X509_STORE_add_cert Failed");
		}
	}

	return rc;
}

void CCmsMessageUtils::free(
		CMS_ContentInfo* contentInfo) {
	if (contentInfo) {
		CMS_ContentInfo_free(contentInfo);
	}
}

void CCmsMessageUtils::free(
		std::deque<X509*> x509Collection) {
	for (TConstIterator<std::deque<X509*> > elemIter(x509Collection); elemIter; elemIter++) {
		X509* elem = *elemIter;
		free(elem);
	}
}

void CCmsMessageUtils::free(
		X509* x509) {
	if (x509) {
		X509_free(x509);
	}
}

void CCmsMessageUtils::free(
		STACK_OF(X509)* x509Stack) {
	if (x509Stack) {
		sk_X509_pop_free(x509Stack, X509_free);
	}
}

void CCmsMessageUtils::free(
		std::deque<BIO*> bioCollection) {
	for (TConstIterator<std::deque<BIO*> > elemIter(bioCollection); elemIter; elemIter++) {
		BIO* elem = *elemIter;
		free(elem);
	}
}

void CCmsMessageUtils::free(
		BIO* bio) {
	if (bio) {
		BIO_vfree(bio);
	}
}

void CCmsMessageUtils::free(
		X509_STORE* x509Store) {
	if (x509Store) {
		X509_STORE_free(x509Store);
	}
}

void CCmsMessageUtils::free(
		EVP_PKEY* privateKey) {
	if (privateKey) {
		EVP_PKEY_free(privateKey);
	}
}

void CCmsMessageUtils::logSslErrors() {
	CAF_CM_STATIC_FUNC_LOG_ONLY("CCmsMessageUtils", "logSslErrors");

	int32 sslErrorCode = ERR_get_error();
	while (sslErrorCode != 0) {
		const char* sslErrorStr = ERR_error_string(sslErrorCode, NULL);
		CAF_CM_LOG_WARN_VA2("SSL Error - code: %d, str: %s", sslErrorCode, sslErrorStr);
		sslErrorCode = ERR_get_error();
	}
}

void CCmsMessageUtils::logCiphers(
		const std::string& prefix,
		const SSL* ssl) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CCmsMessageUtils", "logCiphers");
	CAF_CM_VALIDATE_STRING(prefix);
	CAF_CM_VALIDATE_PTR(ssl);

	int32 index = 0;
	const char* cipher = SSL_get_cipher_list(ssl, index);
	while (cipher != NULL) {
		CAF_CM_LOG_DEBUG_VA3(
				"%s - index: %d, str: %s", prefix.c_str(), index, cipher);
		cipher = SSL_get_cipher_list(ssl, index++);
	}
}
