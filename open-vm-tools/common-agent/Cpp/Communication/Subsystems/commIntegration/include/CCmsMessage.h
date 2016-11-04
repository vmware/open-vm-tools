/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CCmsMessage_h_
#define CCmsMessage_h_

#include <openssl/ssl.h>

#include "Memory/DynamicArray/DynamicArrayInc.h"

namespace Caf {

class CCmsMessage {
public:
	CCmsMessage();
	virtual ~CCmsMessage();

public:
	void initialize(
			const std::string& appId,
			const std::string& pmeId);

public:
	void signBufferToBuffer(
			const SmartPtrCDynamicByteArray& inputBuffer,
			SmartPtrCDynamicByteArray& outputBuffer) const;

	void verifyBufferToBuffer(
			const SmartPtrCDynamicByteArray& inputBuffer,
			SmartPtrCDynamicByteArray& outputBuffer) const;

	void encryptBufferToBuffer(
			const SmartPtrCDynamicByteArray& inputBuffer,
			SmartPtrCDynamicByteArray& outputBuffer) const;

	void decryptBufferToBuffer(
			const SmartPtrCDynamicByteArray& inputBuffer,
			SmartPtrCDynamicByteArray& outputBuffer) const;

	void compressBufferToBuffer(
			const SmartPtrCDynamicByteArray& inputBuffer,
			SmartPtrCDynamicByteArray& outputBuffer) const;

	void uncompressBufferToBuffer(
			const SmartPtrCDynamicByteArray& inputBuffer,
			SmartPtrCDynamicByteArray& outputBuffer) const;

public:
	void signBufferToFile(
			const SmartPtrCDynamicByteArray& inputBuffer,
			const std::string& outputPath) const;

	void verifyBufferToFile(
			const SmartPtrCDynamicByteArray& inputBuffer,
			const std::string& outputPath) const;

	void encryptBufferToFile(
			const SmartPtrCDynamicByteArray& inputBuffer,
			const std::string& outputPath) const;

	void decryptBufferToFile(
			const SmartPtrCDynamicByteArray& inputBuffer,
			const std::string& outputPath) const;

	void compressBufferToFile(
			const SmartPtrCDynamicByteArray& inputBuffer,
			const std::string& outputPath) const;

	void uncompressBufferToFile(
			const SmartPtrCDynamicByteArray& inputBuffer,
			const std::string& outputPath) const;

public:
	void signFileToBuffer(
			const std::string& inputPath,
			SmartPtrCDynamicByteArray& outputBuffer) const;

	void verifyFileToBuffer(
			const std::string& inputPath,
			SmartPtrCDynamicByteArray& outputBuffer) const;

	void encryptFileToBuffer(
			const std::string& inputPath,
			SmartPtrCDynamicByteArray& outputBuffer) const;

	void decryptFileToBuffer(
			const std::string& inputPath,
			SmartPtrCDynamicByteArray& outputBuffer) const;

	void compressFileToBuffer(
			const std::string& inputPath,
			SmartPtrCDynamicByteArray& outputBuffer) const;

	void uncompressFileToBuffer(
			const std::string& inputPath,
			SmartPtrCDynamicByteArray& outputBuffer) const;

public:
	void signFileToFile(
			const std::string& inputPath,
			const std::string& outputPath) const;

	void verifyFileToFile(
			const std::string& inputPath,
			const std::string& outputPath) const;

	void encryptFileToFile(
			const std::string& inputPath,
			const std::string& outputPath) const;

	void decryptFileToFile(
			const std::string& inputPath,
			const std::string& outputPath) const;

	void compressFileToFile(
			const std::string& inputPath,
			const std::string& outputPath) const;

	void uncompressFileToFile(
			const std::string& inputPath,
			const std::string& outputPath) const;

private:
	void sign(
			const SmartPtrCDynamicByteArray& inputBuffer,
			const std::string& inputPath,
			SmartPtrCDynamicByteArray& outputBuffer,
			const std::string& outputPath) const;

	void verify(
			const SmartPtrCDynamicByteArray& inputBuffer,
			const std::string& inputPath,
			SmartPtrCDynamicByteArray& outputBuffer,
			const std::string& outputPath) const;

	void encrypt(
			const SmartPtrCDynamicByteArray& inputBuffer,
			const std::string& inputPath,
			SmartPtrCDynamicByteArray& outputBuffer,
			const std::string& outputPath) const;

	void decrypt(
			const SmartPtrCDynamicByteArray& inputBuffer,
			const std::string& inputPath,
			SmartPtrCDynamicByteArray& outputBuffer,
			const std::string& outputPath) const;

	void compress(
			const SmartPtrCDynamicByteArray& inputBuffer,
			const std::string& inputPath,
			SmartPtrCDynamicByteArray& outputBuffer,
			const std::string& outputPath) const;

	void uncompress(
			const SmartPtrCDynamicByteArray& inputBuffer,
			const std::string& inputPath,
			SmartPtrCDynamicByteArray& outputBuffer,
			const std::string& outputPath) const;

private:
	void checkCrlf(
			const std::string& funcName,
			const std::string& direction,
			const SmartPtrCDynamicByteArray& buffer,
			const std::string& path) const;

	void checkCrlf(
			const std::string& funcName,
			const std::string& direction,
			const SmartPtrCDynamicByteArray& buffer) const;

	void checkCrlf(
			const std::string& funcName,
			const std::string& direction,
			const std::string& path) const;

private:
	std::string getReqDirPath(
			const std::string& directory,
			const std::string& subdir,
			const std::string& subdir1 = std::string()) const;

	std::string getReqFilePath(
			const std::string& directory,
			const std::string& filename) const;

	Cdeqstr getReqFilePaths(
			const std::string& directory,
			const std::string& subdir) const;

	std::string getReqRmtCertsDir(
			const std::string& appId,
			const std::string& pmeId) const;

	void getExistingDir(
			const std::string& parentDir,
			const std::string& childDir,
			std::string& result,
			std::string& errDirs) const;

private:
	bool _isInitialized;

	EVP_CIPHER* _cipher;
	std::string _persistenceDir;

	std::string _nullPath;
	SmartPtrCDynamicByteArray _nullBuffer;

	std::string _encryptPublicKeyPath;
	std::string _decryptPublicKeyPath;
	std::string _decryptPrivateKeyPath;
	std::string _signPublicKeyPath;
	std::string _signPrivateKeyPath;
	Cdeqstr _caCertificatePaths;

	bool _checkCrlf;

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(CCmsMessage);
};

CAF_DECLARE_SMART_POINTER(CCmsMessage);

}

#endif // #ifndef CCmsMessage_h_
