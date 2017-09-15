/*
 *	 Author: bwilliams
 *  Created: Jan 21, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CFILESYSTEMUTILS_H_
#define CFILESYSTEMUTILS_H_



#include "Memory/DynamicArray/DynamicArrayInc.h"

namespace Caf {

class COMMONAGGREGATOR_LINKAGE FileSystemUtils {
public:
	// Helpful typedefs

	// file or directory name only: no path.
	typedef std::deque<std::string> Files;
	typedef Files Directories;

	// directory and file names only: no path.
	struct DirectoryItems {
		DirectoryItems() {};
		DirectoryItems(const Directories& d, const Files& f) : directories(d), files(f) {};

		Directories directories;
		Files files;
	};

	// first is full path to the items in DirectoryItems
	struct PathAndDirectoryItems {
		PathAndDirectoryItems() {};
		PathAndDirectoryItems(const std::string& p, DirectoryItems& i) : path(p), items(i) {};

		std::string		path;
		DirectoryItems	items;
	};

	typedef enum {
		FILE_MODE_REPLACE,
		FILE_MODE_FAIL,
		FILE_MODE_IGNORE
	} FILE_MODE_TYPE;

	typedef std::deque<PathAndDirectoryItems> PathAndDirectoryItemsCollection;

	// Use this constant to match all item names in a directory
	static const std::string REGEX_MATCH_ALL;

	static void createDirectory(const std::string &path, const uint32 mode = 0770);

	static void removeDirectory(const std::string& path);

	static void recursiveRemoveDirectory(const std::string& path);

	static void removeFile(const std::string& path);

	static Files removeFilesInDirectory(const std::string& path, const std::string& regex);

	static bool doesFileExist(const std::string& path);

	static bool doesDirectoryExist(const std::string& path);

	static bool isRegularFile(const std::string& path);

	static std::string getCurrentDir();

	static std::string getCurrentFile();

	static std::string getBasename(const std::string& path);

	static std::string getDirname(const std::string& path);

	static std::string getTmpDir();

	static std::string buildPath(
		const std::string& path,
		const std::string& newElement);

	static std::string buildPath(
		const std::string& path,
		const std::string& newElement1,
		const std::string& newElement2);

	static std::string buildPath(
		const std::string& path,
		const std::string& newElement1,
		const std::string& newElement2,
		const std::string& newElement3);

	static std::string buildPath(
		const std::string& path,
		const std::string& newElement1,
		const std::string& newElement2,
		const std::string& newElement3,
		const std::string& newElement4);

	static std::string loadTextFile(
			const std::string& path);

	static Cdeqstr loadTextFileIntoColl(
			const std::string& path);

	static SmartPtrCDynamicByteArray loadByteFile(
			const std::string& path);

	static void saveTextFile(
		const std::string& outputDir,
		const std::string& filename,
		const std::string& contents,
		const FILE_MODE_TYPE fileMode = FILE_MODE_REPLACE,
		const std::string temporaryFileSuffix = ".tmp");

	static void saveTextFile(
		const std::string& filePath,
		const std::string& contents,
		const FILE_MODE_TYPE fileMode = FILE_MODE_REPLACE,
		const std::string temporaryFileSuffix = ".tmp");

	static void saveByteFile(
		const std::string& filePath,
		const SmartPtrCDynamicByteArray& contents,
		const FILE_MODE_TYPE fileMode = FILE_MODE_REPLACE,
		const std::string temporaryFileSuffix = ".tmp");

	static void saveByteFile(
		const std::string& filePath,
		const byte* contents,
		const size_t& contentsLen,
		const FILE_MODE_TYPE fileMode = FILE_MODE_REPLACE,
		const std::string temporaryFileSuffix = ".tmp");

	static DirectoryItems itemsInDirectory(
		const std::string& path,
		const std::string& regex);

	static PathAndDirectoryItemsCollection recursiveItemsInDirectory(
		const std::string& path,
		const std::string& regex);

	// The caller must ensure that dstPath exists.
	static void copyFile(const std::string& srcPath, const std::string& dstPath);

	static void moveFile(const std::string& srcPath, const std::string& dstPath);

	static void copyDirectory(const std::string& srcPath, const std::string& dstPath);

	static void recursiveCopyDirectory(const std::string& srcPath, const std::string& dstPath);

	static std::string findOptionalFile(
		const std::string& outputDir,
		const std::string& filename);

	static std::string findRequiredFile(
		const std::string& outputDir,
		const std::string& filename);

	static std::deque<std::string> findOptionalFiles(
		const std::string& outputDir,
		const std::string& filename);

	static std::deque<std::string> findRequiredFiles(
		const std::string& outputDir,
		const std::string& filename);

	static void chmod(
		const std::string &path,
		const uint32 mode = 0770);

	static std::string normalizePathForPlatform(
		const std::string &path);

	static std::string normalizePathWithForward(
		const std::string &path);

	static int64 getFileSize(const std::string& filename);
	
	static std::string saveTempTextFile(const std::string& filename_template, const std::string& contents);
	
	static std::string getTempFilename(const std::string& filename_template);

private:
	static void saveFileSafely(
		const std::string& filePath,
		const byte* contents,
		const size_t& contentsLen,
		const std::string& temporaryFileSuffix);

	static void saveByteFileRaw(
		const std::string& path,
		const byte* contents,
		const size_t& contentsLen);


private:
	CAF_CM_DECLARE_NOCREATE(FileSystemUtils);
};

}

#endif /* CFILESYSTEMUTILS_H_ */
