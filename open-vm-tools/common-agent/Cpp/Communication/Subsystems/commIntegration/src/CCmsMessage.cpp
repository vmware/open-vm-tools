/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Memory/DynamicArray/DynamicArrayInc.h"
#include "CCmsMessage.h"
#include "Exception/CCafException.h"
#include "CCmsMessageUtils.h"
#include <fstream>

using namespace Caf;

CCmsMessage::CCmsMessage() :
	_isInitialized(false),
	_cipher(NULL),
	_checkCrlf(false),
	CAF_CM_INIT_LOG("CCmsMessage") {
}

CCmsMessage::~CCmsMessage() {
}

void CCmsMessage::initialize(
		const std::string& appId,
		const std::string& pmeId) {
	CAF_CM_FUNCNAME_VALIDATE("initialize");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(appId);
	CAF_CM_VALIDATE_STRING(pmeId);

	SSL_library_init();
	SSL_load_error_strings();

	_persistenceDir = AppConfigUtils::getRequiredString("persistence_dir");

	const std::string locDir = getReqDirPath(_persistenceDir, "local");
	const std::string locPublicKeyPath = getReqFilePath(locDir, "cert.pem");
	const std::string locPrivateKeyPath = getReqFilePath(locDir, "privateKey.pem");

	const std::string rmtCertsDir = getReqRmtCertsDir(appId, pmeId);
	const std::string rmtPublicKeyPath = getReqFilePath(rmtCertsDir, "cmsCert.pem");
	const std::string rmtCmsCipherNamePath = getReqFilePath(rmtCertsDir, "cmsCipherName.txt");
	const std::string rmtCipherName = FileSystemUtils::loadTextFile(rmtCmsCipherNamePath);

	_caCertificatePaths = getReqFilePaths(rmtCertsDir, "cmsCertCollection");

	_cipher = const_cast<EVP_CIPHER*>(EVP_get_cipherbyname(rmtCipherName.c_str()));
	CAF_CM_VALIDATE_PTR(_cipher);

	_encryptPublicKeyPath = rmtPublicKeyPath;
	_decryptPublicKeyPath = rmtPublicKeyPath;
	_decryptPrivateKeyPath = locPrivateKeyPath;
	_signPublicKeyPath = locPublicKeyPath;
	_signPrivateKeyPath = locPrivateKeyPath;

	CAF_CM_LOG_DEBUG_VA1("Initializing - rmtCipherName: %s", rmtCipherName.c_str());
	CAF_CM_LOG_DEBUG_VA1("Initializing - encryptPublicKeyPath: %s", _encryptPublicKeyPath.c_str());
	CAF_CM_LOG_DEBUG_VA1("Initializing - decryptPublicKeyPath: %s", _decryptPublicKeyPath.c_str());
	CAF_CM_LOG_DEBUG_VA1("Initializing - decryptPrivateKeyPath: %s", _decryptPrivateKeyPath.c_str());
	CAF_CM_LOG_DEBUG_VA1("Initializing - signPublicKeyPath: %s", _signPublicKeyPath.c_str());
	CAF_CM_LOG_DEBUG_VA1("Initializing - signPrivateKeyPath: %s", _signPrivateKeyPath.c_str());
	CAF_CM_LOG_DEBUG_VA2("Initializing - caCertificatePath: %s, %s", rmtCertsDir.c_str(), "cmsCertCollection");

	_checkCrlf = AppConfigUtils::getOptionalBoolean("security", "check_crlf");

	_isInitialized = true;
}

void CCmsMessage::signBufferToBuffer(
		const SmartPtrCDynamicByteArray& inputBuffer,
		SmartPtrCDynamicByteArray& outputBuffer) const {
	CAF_CM_FUNCNAME_VALIDATE("signBufferToBuffer");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_SMARTPTR(inputBuffer);

	CAF_CM_LOG_DEBUG_VA1("%s", CAF_CM_GET_FUNCNAME);

	outputBuffer.CreateInstance();
	sign(inputBuffer, _nullPath, outputBuffer, _nullPath);
}

void CCmsMessage::verifyBufferToBuffer(
		const SmartPtrCDynamicByteArray& inputBuffer,
		SmartPtrCDynamicByteArray& outputBuffer) const {
	CAF_CM_FUNCNAME_VALIDATE("verifyBufferToBuffer");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_SMARTPTR(inputBuffer);

	CAF_CM_LOG_DEBUG_VA1("%s", CAF_CM_GET_FUNCNAME);

	outputBuffer.CreateInstance();
	verify(inputBuffer, _nullPath, outputBuffer, _nullPath);
}

void CCmsMessage::encryptBufferToBuffer(
		const SmartPtrCDynamicByteArray& inputBuffer,
		SmartPtrCDynamicByteArray& outputBuffer) const {
	CAF_CM_FUNCNAME_VALIDATE("encryptBufferToBuffer");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_SMARTPTR(inputBuffer);

	CAF_CM_LOG_DEBUG_VA1("%s", CAF_CM_GET_FUNCNAME);

	outputBuffer.CreateInstance();
	encrypt(inputBuffer, _nullPath, outputBuffer, _nullPath);
}

void CCmsMessage::decryptBufferToBuffer(
		const SmartPtrCDynamicByteArray& inputBuffer,
		SmartPtrCDynamicByteArray& outputBuffer) const {
	CAF_CM_FUNCNAME_VALIDATE("decryptBufferToBuffer");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_SMARTPTR(inputBuffer);

	CAF_CM_LOG_DEBUG_VA1("%s", CAF_CM_GET_FUNCNAME);

	outputBuffer.CreateInstance();
	decrypt(inputBuffer, _nullPath, outputBuffer, _nullPath);
}

void CCmsMessage::compressBufferToBuffer(
		const SmartPtrCDynamicByteArray& inputBuffer,
		SmartPtrCDynamicByteArray& outputBuffer) const {
	CAF_CM_FUNCNAME_VALIDATE("compressBufferToBuffer");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_SMARTPTR(inputBuffer);

	CAF_CM_LOG_DEBUG_VA1("%s", CAF_CM_GET_FUNCNAME);

	outputBuffer.CreateInstance();
	compress(inputBuffer, _nullPath, outputBuffer, _nullPath);
}

void CCmsMessage::uncompressBufferToBuffer(
		const SmartPtrCDynamicByteArray& inputBuffer,
		SmartPtrCDynamicByteArray& outputBuffer) const {
	CAF_CM_FUNCNAME_VALIDATE("uncompressBufferToBuffer");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_SMARTPTR(inputBuffer);

	CAF_CM_LOG_DEBUG_VA1("%s", CAF_CM_GET_FUNCNAME);

	outputBuffer.CreateInstance();
	uncompress(inputBuffer, _nullPath, outputBuffer, _nullPath);
}

void CCmsMessage::signBufferToFile(
		const SmartPtrCDynamicByteArray& inputBuffer,
		const std::string& outputPath) const {
	CAF_CM_FUNCNAME_VALIDATE("signBufferToFile");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_SMARTPTR(inputBuffer);
	CAF_CM_VALIDATE_STRING(outputPath);

	CAF_CM_LOG_DEBUG_VA2("%s - %s", CAF_CM_GET_FUNCNAME, outputPath.c_str());

	SmartPtrCDynamicByteArray nullBuffer;
	sign(inputBuffer, _nullPath, nullBuffer, outputPath);
}

void CCmsMessage::verifyBufferToFile(
		const SmartPtrCDynamicByteArray& inputBuffer,
		const std::string& outputPath) const {
	CAF_CM_FUNCNAME_VALIDATE("verifyBufferToFile");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_SMARTPTR(inputBuffer);
	CAF_CM_VALIDATE_STRING(outputPath);

	CAF_CM_LOG_DEBUG_VA2("%s - %s", CAF_CM_GET_FUNCNAME, outputPath.c_str());

	SmartPtrCDynamicByteArray nullBuffer;
	verify(inputBuffer, _nullPath, nullBuffer, outputPath);
}

void CCmsMessage::encryptBufferToFile(
		const SmartPtrCDynamicByteArray& inputBuffer,
		const std::string& outputPath) const {
	CAF_CM_FUNCNAME_VALIDATE("encryptBufferToFile");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_SMARTPTR(inputBuffer);
	CAF_CM_VALIDATE_STRING(outputPath);

	CAF_CM_LOG_DEBUG_VA2("%s - %s", CAF_CM_GET_FUNCNAME, outputPath.c_str());

	SmartPtrCDynamicByteArray nullBuffer;
	encrypt(inputBuffer, _nullPath, nullBuffer, outputPath);
}

void CCmsMessage::decryptBufferToFile(
		const SmartPtrCDynamicByteArray& inputBuffer,
		const std::string& outputPath) const {
	CAF_CM_FUNCNAME_VALIDATE("decryptBufferToFile");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_SMARTPTR(inputBuffer);
	CAF_CM_VALIDATE_STRING(outputPath);

	CAF_CM_LOG_DEBUG_VA2("%s - %s", CAF_CM_GET_FUNCNAME, outputPath.c_str());

	SmartPtrCDynamicByteArray nullBuffer;
	decrypt(inputBuffer, _nullPath, nullBuffer, outputPath);
}

void CCmsMessage::compressBufferToFile(
		const SmartPtrCDynamicByteArray& inputBuffer,
		const std::string& outputPath) const {
	CAF_CM_FUNCNAME_VALIDATE("compressBufferToFile");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_SMARTPTR(inputBuffer);
	CAF_CM_VALIDATE_STRING(outputPath);

	CAF_CM_LOG_DEBUG_VA2("%s - %s", CAF_CM_GET_FUNCNAME, outputPath.c_str());

	SmartPtrCDynamicByteArray nullBuffer;
	compress(inputBuffer, _nullPath, nullBuffer, outputPath);
}

void CCmsMessage::uncompressBufferToFile(
		const SmartPtrCDynamicByteArray& inputBuffer,
		const std::string& outputPath) const {
	CAF_CM_FUNCNAME_VALIDATE("uncompressBufferToFile");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_SMARTPTR(inputBuffer);
	CAF_CM_VALIDATE_STRING(outputPath);

	CAF_CM_LOG_DEBUG_VA2("%s - %s", CAF_CM_GET_FUNCNAME, outputPath.c_str());

	SmartPtrCDynamicByteArray nullBuffer;
	uncompress(inputBuffer, _nullPath, nullBuffer, outputPath);
}

void CCmsMessage::signFileToBuffer(
		const std::string& inputPath,
		SmartPtrCDynamicByteArray& outputBuffer) const {
	CAF_CM_FUNCNAME_VALIDATE("signFileToBuffer");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(inputPath);

	CAF_CM_LOG_DEBUG_VA2("%s - %s", CAF_CM_GET_FUNCNAME, inputPath.c_str());

	outputBuffer.CreateInstance();
	sign(_nullBuffer, inputPath, outputBuffer, _nullPath);
}

void CCmsMessage::verifyFileToBuffer(
		const std::string& inputPath,
		SmartPtrCDynamicByteArray& outputBuffer) const {
	CAF_CM_FUNCNAME_VALIDATE("verifyFileToBuffer");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(inputPath);

	CAF_CM_LOG_DEBUG_VA2("%s - %s", CAF_CM_GET_FUNCNAME, inputPath.c_str());

	outputBuffer.CreateInstance();
	verify(_nullBuffer, inputPath, outputBuffer, _nullPath);
}

void CCmsMessage::encryptFileToBuffer(
		const std::string& inputPath,
		SmartPtrCDynamicByteArray& outputBuffer) const {
	CAF_CM_FUNCNAME_VALIDATE("encryptFileToBuffer");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(inputPath);

	CAF_CM_LOG_DEBUG_VA2("%s - %s", CAF_CM_GET_FUNCNAME, inputPath.c_str());

	outputBuffer.CreateInstance();
	encrypt(_nullBuffer, inputPath, outputBuffer, _nullPath);
}

void CCmsMessage::decryptFileToBuffer(
		const std::string& inputPath,
		SmartPtrCDynamicByteArray& outputBuffer) const {
	CAF_CM_FUNCNAME_VALIDATE("decryptFileToBuffer");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(inputPath);

	CAF_CM_LOG_DEBUG_VA2("%s - %s", CAF_CM_GET_FUNCNAME, inputPath.c_str());

	outputBuffer.CreateInstance();
	decrypt(_nullBuffer, inputPath, outputBuffer, _nullPath);
}

void CCmsMessage::compressFileToBuffer(
		const std::string& inputPath,
		SmartPtrCDynamicByteArray& outputBuffer) const {
	CAF_CM_FUNCNAME_VALIDATE("compressFileToBuffer");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(inputPath);

	CAF_CM_LOG_DEBUG_VA2("%s - %s", CAF_CM_GET_FUNCNAME, inputPath.c_str());

	outputBuffer.CreateInstance();
	compress(_nullBuffer, inputPath, outputBuffer, _nullPath);
}

void CCmsMessage::uncompressFileToBuffer(
		const std::string& inputPath,
		SmartPtrCDynamicByteArray& outputBuffer) const {
	CAF_CM_FUNCNAME_VALIDATE("uncompressFileToBuffer");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(inputPath);

	CAF_CM_LOG_DEBUG_VA2("%s - %s", CAF_CM_GET_FUNCNAME, inputPath.c_str());

	outputBuffer.CreateInstance();
	uncompress(_nullBuffer, inputPath, outputBuffer, _nullPath);
}

void CCmsMessage::signFileToFile(
		const std::string& inputPath,
		const std::string& outputPath) const {
	CAF_CM_FUNCNAME_VALIDATE("signFileToFile");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(inputPath);
	CAF_CM_VALIDATE_STRING(outputPath);

	CAF_CM_LOG_DEBUG_VA3("%s - %s, %s", CAF_CM_GET_FUNCNAME, inputPath.c_str(),
			outputPath.c_str());

	SmartPtrCDynamicByteArray nullBuffer;
	sign(_nullBuffer, inputPath, nullBuffer, outputPath);
}

void CCmsMessage::verifyFileToFile(
		const std::string& inputPath,
		const std::string& outputPath) const {
	CAF_CM_FUNCNAME_VALIDATE("verifyFileToFile");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(inputPath);
	CAF_CM_VALIDATE_STRING(outputPath);

	CAF_CM_LOG_DEBUG_VA3("%s - %s, %s", CAF_CM_GET_FUNCNAME, inputPath.c_str(),
			outputPath.c_str());

	SmartPtrCDynamicByteArray nullBuffer;
	verify(_nullBuffer, inputPath, nullBuffer, outputPath);
}

void CCmsMessage::encryptFileToFile(
		const std::string& inputPath,
		const std::string& outputPath) const {
	CAF_CM_FUNCNAME_VALIDATE("encryptFileToFile");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(inputPath);
	CAF_CM_VALIDATE_STRING(outputPath);

	CAF_CM_LOG_DEBUG_VA3("%s - %s, %s", CAF_CM_GET_FUNCNAME, inputPath.c_str(),
			outputPath.c_str());

	SmartPtrCDynamicByteArray nullBuffer;
	encrypt(_nullBuffer, inputPath, nullBuffer, outputPath);
}

void CCmsMessage::decryptFileToFile(
		const std::string& inputPath,
		const std::string& outputPath) const {
	CAF_CM_FUNCNAME_VALIDATE("decryptFileToFile");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(inputPath);
	CAF_CM_VALIDATE_STRING(outputPath);

	CAF_CM_LOG_DEBUG_VA3("%s - %s, %s", CAF_CM_GET_FUNCNAME, inputPath.c_str(),
			outputPath.c_str());

	SmartPtrCDynamicByteArray nullBuffer;
	decrypt(_nullBuffer, inputPath, nullBuffer, outputPath);
}

void CCmsMessage::compressFileToFile(
		const std::string& inputPath,
		const std::string& outputPath) const {
	CAF_CM_FUNCNAME_VALIDATE("compressFileToFile");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(inputPath);
	CAF_CM_VALIDATE_STRING(outputPath);

	CAF_CM_LOG_DEBUG_VA3("%s - %s, %s", CAF_CM_GET_FUNCNAME, inputPath.c_str(),
			outputPath.c_str());

	SmartPtrCDynamicByteArray nullBuffer;
	compress(_nullBuffer, inputPath, nullBuffer, outputPath);
}

void CCmsMessage::uncompressFileToFile(
		const std::string& inputPath,
		const std::string& outputPath) const {
	CAF_CM_FUNCNAME_VALIDATE("uncompressFileToFile");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(inputPath);
	CAF_CM_VALIDATE_STRING(outputPath);

	CAF_CM_LOG_DEBUG_VA3("%s - %s, %s", CAF_CM_GET_FUNCNAME, inputPath.c_str(),
			outputPath.c_str());

	SmartPtrCDynamicByteArray nullBuffer;
	uncompress(_nullBuffer, inputPath, nullBuffer, outputPath);
}

void CCmsMessage::sign(
		const SmartPtrCDynamicByteArray& inputBuffer,
		const std::string& inputPath,
		SmartPtrCDynamicByteArray& outputBuffer,
		const std::string& outputPath) const {
	CAF_CM_FUNCNAME("sign");

	checkCrlf(CAF_CM_GET_FUNCNAME, "input", inputBuffer, inputPath);

	const uint32 flags = CMS_STREAM | CMS_BINARY;

	BIO* signPublicKeyBio = NULL;
	BIO* signPrivateKeyBio = NULL;
	X509* signPublicKey = NULL;
	EVP_PKEY* signPrivateKey = NULL;

	BIO* outputBio = NULL;
	BIO* inputBufferBio = NULL;
	CMS_ContentInfo* contentInfo = NULL;
	try {
		signPublicKeyBio = CCmsMessageUtils::inputFileToBio(_signPublicKeyPath);
		signPrivateKeyBio = CCmsMessageUtils::inputFileToBio(_signPrivateKeyPath);

		signPublicKey = CCmsMessageUtils::bioToX509(signPublicKeyBio);
		// Casting to void to ignore return value and eliminate compiler warning
		(void) BIO_reset(signPublicKeyBio);
		signPrivateKey = CCmsMessageUtils::bioToPrivateKey(signPrivateKeyBio);

		inputBufferBio = CCmsMessageUtils::inputToBio(inputBuffer, inputPath);

		contentInfo = CMS_sign(signPublicKey, signPrivateKey, NULL, inputBufferBio,
				flags);
		if (! contentInfo) {
			CCmsMessageUtils::logSslErrors();
			CAF_CM_EXCEPTION_VA0(E_FAIL, "CMS_sign Failed");
		}

		outputBio = CCmsMessageUtils::outputToBio(outputBuffer, outputPath);

		/* Write out S/MIME message */
		if (! SMIME_write_CMS(outputBio, contentInfo, inputBufferBio, flags)) {
			CCmsMessageUtils::logSslErrors();
			CAF_CM_EXCEPTION_VA1(E_FAIL,
					"SMIME_write_CMS Failed - %s", outputPath.c_str());
		}

		CCmsMessageUtils::bioToOutput(outputBio, outputBuffer, outputPath);
	}
	CAF_CM_CATCH_CAF
	CAF_CM_CATCH_DEFAULT;

	CCmsMessageUtils::free(contentInfo);
	CCmsMessageUtils::free(signPublicKey);
	CCmsMessageUtils::free(signPrivateKey);
	CCmsMessageUtils::free(inputBufferBio);
	CCmsMessageUtils::free(outputBio);
	CCmsMessageUtils::free(signPublicKeyBio);
	CCmsMessageUtils::free(signPrivateKeyBio);

	CAF_CM_THROWEXCEPTION;

	checkCrlf(CAF_CM_GET_FUNCNAME, "output", outputBuffer, outputPath);
}

void CCmsMessage::verify(
		const SmartPtrCDynamicByteArray& inputBuffer,
		const std::string& inputPath,
		SmartPtrCDynamicByteArray& outputBuffer,
		const std::string& outputPath) const {
	CAF_CM_FUNCNAME("verify");

	checkCrlf(CAF_CM_GET_FUNCNAME, "input", inputBuffer, inputPath);

	std::deque<BIO*> caCertBios;
	std::deque<X509*> caCertX509s;
	X509_STORE* caCertStore = NULL;

	BIO* inputBufferBio = NULL;
	BIO* inputParsedBio = NULL;
	BIO* outputBio = NULL;
	CMS_ContentInfo* contentInfo = NULL;
	try {
		caCertBios = CCmsMessageUtils::inputFilesToBio(_caCertificatePaths);
		caCertX509s = CCmsMessageUtils::biosToX509(caCertBios);
		caCertStore = CCmsMessageUtils::createX509Store(caCertX509s);

		/*
		 * X509_STORE_free will free up recipient Store and its contents so set
		 * caCerts to NULL so it isn't freed up twice.
		 */
		caCertX509s.clear();

		inputBufferBio = CCmsMessageUtils::inputToBio(inputBuffer, inputPath);

		/* parse message */
		contentInfo = SMIME_read_CMS(inputBufferBio, &inputParsedBio);
		if (! contentInfo) {
			CCmsMessageUtils::logSslErrors();
			CAF_CM_EXCEPTION_VA0(E_FAIL, "SMIME_read_CMS Failed");
		}

		outputBio = CCmsMessageUtils::outputToBio(outputBuffer, outputPath);

		if (! CMS_verify(contentInfo, NULL, caCertStore, inputParsedBio, outputBio, 0)) {
			CCmsMessageUtils::logSslErrors();
			CAF_CM_EXCEPTION_VA0(E_FAIL, "CMS_verify Failed");
		}

		CCmsMessageUtils::bioToOutput(outputBio, outputBuffer, outputPath);
	}
	CAF_CM_CATCH_CAF
	CAF_CM_CATCH_DEFAULT;

	CCmsMessageUtils::free(contentInfo);
	CCmsMessageUtils::free(caCertX509s);
	CCmsMessageUtils::free(caCertStore);
	CCmsMessageUtils::free(inputBufferBio);
	CCmsMessageUtils::free(outputBio);
	CCmsMessageUtils::free(inputParsedBio);
	CCmsMessageUtils::free(caCertBios);

	CAF_CM_THROWEXCEPTION;

	checkCrlf(CAF_CM_GET_FUNCNAME, "output", outputBuffer, outputPath);
}

void CCmsMessage::encrypt(
		const SmartPtrCDynamicByteArray& inputBuffer,
		const std::string& inputPath,
		SmartPtrCDynamicByteArray& outputBuffer,
		const std::string& outputPath) const {
	CAF_CM_FUNCNAME("encrypt");

	checkCrlf(CAF_CM_GET_FUNCNAME, "input", inputBuffer, inputPath);

	const uint32 flags = CMS_STREAM | CMS_BINARY;

	BIO* encryptPublicKeyBio = NULL;
	X509* encryptPublicKey = NULL;
	STACK_OF(X509)* encryptPublicKeyStack = NULL;

	BIO* outputBio = NULL;
	BIO* inputBufferBio = NULL;
	CMS_ContentInfo* contentInfo = NULL;
	try {
		encryptPublicKeyBio = CCmsMessageUtils::inputFileToBio(_encryptPublicKeyPath);
		encryptPublicKey = CCmsMessageUtils::bioToX509(encryptPublicKeyBio);
		encryptPublicKeyStack = CCmsMessageUtils::createX509Stack(encryptPublicKey);

		/*
		 * sk_X509_pop_free will CCmsMessageUtils::free up recipient STACK and its contents so set
		 * encryptPublicKey to NULL so it isn't freed up twice.
		 */
		encryptPublicKey = NULL;

		inputBufferBio = CCmsMessageUtils::inputToBio(inputBuffer, inputPath);

		/* encrypt content */
		contentInfo = CMS_encrypt(encryptPublicKeyStack, inputBufferBio, _cipher, flags);
		if (! contentInfo) {
			CCmsMessageUtils::logSslErrors();
			CAF_CM_EXCEPTION_VA0(E_FAIL, "CMS_encrypt Failed");
		}

		outputBio = CCmsMessageUtils::outputToBio(outputBuffer, outputPath);

		/* Write out S/MIME message */
		if (! SMIME_write_CMS(outputBio, contentInfo, inputBufferBio, flags)) {
			CCmsMessageUtils::logSslErrors();
			CAF_CM_EXCEPTION_VA0(E_FAIL, "SMIME_write_CMS Failed");
		}

		CCmsMessageUtils::bioToOutput(outputBio, outputBuffer, outputPath);
	}
	CAF_CM_CATCH_CAF
	CAF_CM_CATCH_DEFAULT;

	CCmsMessageUtils::free(contentInfo);
	CCmsMessageUtils::free(encryptPublicKey);
	CCmsMessageUtils::free(encryptPublicKeyStack);
	CCmsMessageUtils::free(inputBufferBio);
	CCmsMessageUtils::free(outputBio);
	CCmsMessageUtils::free(encryptPublicKeyBio);

	CAF_CM_THROWEXCEPTION;

	checkCrlf(CAF_CM_GET_FUNCNAME, "output", outputBuffer, outputPath);
}

void CCmsMessage::decrypt(
		const SmartPtrCDynamicByteArray& inputBuffer,
		const std::string& inputPath,
		SmartPtrCDynamicByteArray& outputBuffer,
		const std::string& outputPath) const {
	CAF_CM_FUNCNAME("decrypt");

	checkCrlf(CAF_CM_GET_FUNCNAME, "input", inputBuffer, inputPath);

	BIO* decryptPublicKeyBio = NULL;
	BIO* decryptPrivateKeyBio = NULL;
	X509* decryptPublicKey = NULL;
	EVP_PKEY* decryptPrivateKey = NULL;

	BIO* outputBio = NULL;
	BIO* inputBufferBio = NULL;
	CMS_ContentInfo* contentInfo = NULL;
	try {
		decryptPublicKeyBio = CCmsMessageUtils::inputFileToBio(_decryptPublicKeyPath);
		decryptPrivateKeyBio = CCmsMessageUtils::inputFileToBio(_decryptPrivateKeyPath);

		decryptPublicKey = CCmsMessageUtils::bioToX509(decryptPublicKeyBio);
		decryptPrivateKey = CCmsMessageUtils::bioToPrivateKey(decryptPrivateKeyBio);

		inputBufferBio = CCmsMessageUtils::inputToBio(inputBuffer, inputPath);

		contentInfo = SMIME_read_CMS(inputBufferBio, NULL);
		if (! contentInfo) {
			CCmsMessageUtils::logSslErrors();
			CAF_CM_EXCEPTION_VA0(E_FAIL, "SMIME_read_CMS Failed");
		}

		outputBio = CCmsMessageUtils::outputToBio(outputBuffer, outputPath);

		if (!CMS_decrypt(contentInfo, decryptPrivateKey, decryptPublicKey,
				NULL, outputBio, 0)) {
			CCmsMessageUtils::logSslErrors();
			CAF_CM_EXCEPTION_VA0(E_FAIL, "CMS_decrypt Failed");
		}

		CCmsMessageUtils::bioToOutput(outputBio, outputBuffer, outputPath);
	}
	CAF_CM_CATCH_CAF
	CAF_CM_CATCH_DEFAULT;

	CCmsMessageUtils::free(contentInfo);
	CCmsMessageUtils::free(decryptPublicKey);
	CCmsMessageUtils::free(decryptPrivateKey);
	CCmsMessageUtils::free(inputBufferBio);
	CCmsMessageUtils::free(outputBio);
	CCmsMessageUtils::free(decryptPublicKeyBio);
	CCmsMessageUtils::free(decryptPrivateKeyBio);

	CAF_CM_THROWEXCEPTION;

	checkCrlf(CAF_CM_GET_FUNCNAME, "output", outputBuffer, outputPath);
}

void CCmsMessage::compress(
		const SmartPtrCDynamicByteArray& inputBuffer,
		const std::string& inputPath,
		SmartPtrCDynamicByteArray& outputBuffer,
		const std::string& outputPath) const {
	CAF_CM_FUNCNAME("compress");

	checkCrlf(CAF_CM_GET_FUNCNAME, "input", inputBuffer, inputPath);

	BIO* inputBufferBio = NULL;
	BIO* outputBio = NULL;
	CMS_ContentInfo* contentInfo = NULL;
	try {
		inputBufferBio = CCmsMessageUtils::inputToBio(inputBuffer, inputPath);

		/* parse message */
		contentInfo = CMS_compress(inputBufferBio, NID_zlib_compression,
				CMS_STREAM | CMS_BINARY);
		if (! contentInfo) {
			CCmsMessageUtils::logSslErrors();
			CAF_CM_EXCEPTION_VA0(E_FAIL, "CMS_compress Failed");
		}

		outputBio = CCmsMessageUtils::outputToBio(outputBuffer, outputPath);

		/* Write out S/MIME message */
		if (! SMIME_write_CMS(outputBio, contentInfo, inputBufferBio, CMS_STREAM)) {
			CCmsMessageUtils::logSslErrors();
			CAF_CM_EXCEPTION_VA0(E_FAIL, "SMIME_write_CMS Failed");
		}

		CCmsMessageUtils::bioToOutput(outputBio, outputBuffer, outputPath);
	}
	CAF_CM_CATCH_CAF
	CAF_CM_CATCH_DEFAULT;

	CCmsMessageUtils::free(contentInfo);
	CCmsMessageUtils::free(inputBufferBio);
	CCmsMessageUtils::free(outputBio);

	CAF_CM_THROWEXCEPTION;

	checkCrlf(CAF_CM_GET_FUNCNAME, "output", outputBuffer, outputPath);
}

void CCmsMessage::uncompress(
		const SmartPtrCDynamicByteArray& inputBuffer,
		const std::string& inputPath,
		SmartPtrCDynamicByteArray& outputBuffer,
		const std::string& outputPath) const {
	CAF_CM_FUNCNAME("uncompress");

	checkCrlf(CAF_CM_GET_FUNCNAME, "input", inputBuffer, inputPath);

	BIO* inputBufferBio = NULL;
	BIO* outputBio = NULL;
	CMS_ContentInfo* contentInfo = NULL;
	try {
		inputBufferBio = CCmsMessageUtils::inputToBio(inputBuffer, inputPath);

		/* parse message */
		contentInfo = SMIME_read_CMS(inputBufferBio, NULL);
		if (! contentInfo) {
			CCmsMessageUtils::logSslErrors();
			CAF_CM_EXCEPTION_VA0(E_FAIL, "SMIME_read_CMS Failed");
		}

		outputBio = CCmsMessageUtils::outputToBio(outputBuffer, outputPath);

		if (! CMS_uncompress(contentInfo, outputBio, NULL, 0)) {
			CCmsMessageUtils::logSslErrors();
			CAF_CM_EXCEPTION_VA0(E_FAIL, "CMS_uncompress Failed");
		}

		CCmsMessageUtils::bioToOutput(outputBio, outputBuffer, outputPath);
	}
	CAF_CM_CATCH_CAF
	CAF_CM_CATCH_DEFAULT;

	CCmsMessageUtils::free(contentInfo);
	CCmsMessageUtils::free(inputBufferBio);
	CCmsMessageUtils::free(outputBio);

	CAF_CM_THROWEXCEPTION;

	checkCrlf(CAF_CM_GET_FUNCNAME, "output", outputBuffer, outputPath);
}

void CCmsMessage::checkCrlf(
		const std::string& funcName,
		const std::string& direction,
		const SmartPtrCDynamicByteArray& buffer,
		const std::string& path) const {
	CAF_CM_FUNCNAME("checkCrlf");
	CAF_CM_VALIDATE_STRING(funcName);
	CAF_CM_VALIDATE_STRING(direction);

	if (_checkCrlf) {
		if (! buffer.IsNull()) {
			checkCrlf(funcName, direction, buffer);
		} else if (! path.empty()) {
			checkCrlf(funcName, direction, path);
		} else {
			CAF_CM_EXCEPTION_VA0(E_FAIL, "Must provide buffer or path");
		}
	}
}

void CCmsMessage::checkCrlf(
		const std::string& funcName,
		const std::string& direction,
		const SmartPtrCDynamicByteArray& buffer) const {
	CAF_CM_FUNCNAME("checkCrlf(buffer)");
	CAF_CM_VALIDATE_STRING(funcName);
	CAF_CM_VALIDATE_STRING(direction);
	CAF_CM_VALIDATE_SMARTPTR(buffer);

	bool isFnd = false;
	for (uint32 i = 0; ! isFnd && (i < buffer->getElementCount()); i++) {
		if (buffer->getAt(i) == '\r') {
			CAF_CM_EXCEPTION_VA2(E_FAIL, "Found CRLF - func: %s, dir: %s",
					funcName.c_str(), direction.c_str());
		}
	}
}

void CCmsMessage::checkCrlf(
		const std::string& funcName,
		const std::string& direction,
		const std::string& path) const {
	CAF_CM_FUNCNAME("checkCrlf(path)");
	CAF_CM_VALIDATE_STRING(funcName);
	CAF_CM_VALIDATE_STRING(direction);
	CAF_CM_VALIDATE_STRING(path);

	std::ifstream ifs(path.c_str());
	if(!ifs) {
		CAF_CM_EXCEPTION_VA1(E_FAIL, "Failed to open file - %s", path.c_str());
	}

	std::istream::sentry se(ifs, true);
	std::streambuf* sb = ifs.rdbuf();

	char c = '\0';
	bool isFnd = false;
	while (! isFnd && ! ifs.eof() && (c != EOF)) {
		c = sb->sbumpc();
		if (c == '\r') {
			CAF_CM_EXCEPTION_VA3(E_FAIL, "Found CRLF - func: %s, dir: %s, path: %s",
					funcName.c_str(), direction.c_str(), path.c_str());
		}
	}
}

std::string CCmsMessage::getReqDirPath(
		const std::string& directory,
		const std::string& subdir,
		const std::string& subdir1) const {
	CAF_CM_FUNCNAME("getReqFilePath");
	CAF_CM_VALIDATE_STRING(directory);
	CAF_CM_VALIDATE_STRING(subdir);

	std::string rc;
	if (subdir1.empty()) {
		rc = FileSystemUtils::buildPath(directory, subdir);
	} else {
		rc = FileSystemUtils::buildPath(directory, subdir, subdir1);
	}

	if (! FileSystemUtils::doesDirectoryExist(rc)) {
		CAF_CM_EXCEPTION_VA1(ERROR_FILE_NOT_FOUND,
				"Directory does not exist - %s", rc.c_str());
	}

	return rc;
}

std::string CCmsMessage::getReqFilePath(
		const std::string& directory,
		const std::string& filename) const {
	CAF_CM_FUNCNAME("getReqFilePath");
	CAF_CM_VALIDATE_STRING(directory);
	CAF_CM_VALIDATE_STRING(filename);

	const std::string rc = FileSystemUtils::buildPath(directory, filename);
	if (! FileSystemUtils::doesFileExist(rc)) {
		CAF_CM_EXCEPTION_VA1(ERROR_FILE_NOT_FOUND,
				"File does not exist - %s", rc.c_str());
	}

	return rc;
}

Cdeqstr CCmsMessage::getReqFilePaths(
		const std::string& directory,
		const std::string& subdir) const {
	CAF_CM_FUNCNAME_VALIDATE("getReqFilePaths");
	CAF_CM_VALIDATE_STRING(directory);
	CAF_CM_VALIDATE_STRING(subdir);

	const std::string dirPath = getReqDirPath(directory, subdir);

	FileSystemUtils::DirectoryItems dirItems = FileSystemUtils::itemsInDirectory(
			dirPath, FileSystemUtils::REGEX_MATCH_ALL);

	Cdeqstr rc;
	const FileSystemUtils::Files files = dirItems.files;
	for (TConstIterator<FileSystemUtils::Files> fileIter(files); fileIter; fileIter++) {
		const std::string filename = *fileIter;
		const std::string filePath = FileSystemUtils::buildPath(
				dirPath, filename);

		rc.push_back(filePath);
	}

	return rc;
}

std::string CCmsMessage::getReqRmtCertsDir(
		const std::string& appId,
		const std::string& pmeId) const {
	CAF_CM_FUNCNAME("getReqRmtCertsDir");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(appId);
	CAF_CM_VALIDATE_STRING(pmeId);

	const std::string rmtCertsDir = getReqDirPath(_persistenceDir, "remote");
	const std::string pmeIdLower = CStringUtils::toLower(pmeId);
	const std::string appIdLower = CStringUtils::toLower(appId);
	const std::string pmeIdUpper = CStringUtils::toUpper(pmeId);
	const std::string appIdUpper = CStringUtils::toUpper(appId);

	std::string rc;
	std::string errDirs;
	getExistingDir(rmtCertsDir, pmeIdLower, rc, errDirs);
	getExistingDir(rmtCertsDir, appIdLower, rc, errDirs);
	getExistingDir(rmtCertsDir, pmeIdUpper, rc, errDirs);
	getExistingDir(rmtCertsDir, appIdUpper, rc, errDirs);
	getExistingDir(rmtCertsDir, "remote_default", rc, errDirs);

	if (rc.empty()) {
		CAF_CM_EXCEPTION_VA1(ERROR_FILE_NOT_FOUND,
				"Remote directories do not exist - %s", errDirs.c_str());
	}

	return rc;
}

void CCmsMessage::getExistingDir(
		const std::string& parentDir,
		const std::string& childDir,
		std::string& result,
		std::string& errDirs) const {
	CAF_CM_FUNCNAME_VALIDATE("getExistingDir");
	CAF_CM_VALIDATE_STRING(parentDir);
	CAF_CM_VALIDATE_STRING(childDir);

	if (result.empty()) {
		const std::string finalDir = FileSystemUtils::buildPath(parentDir, childDir);
		errDirs += finalDir + ", ";
		if (FileSystemUtils::doesDirectoryExist(finalDir)) {
			result = finalDir;
		}
	}
}
