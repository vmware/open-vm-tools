/*
 *	Author: bwilliams
 *  Created: Jan 21, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Memory/DynamicArray/DynamicArrayInc.h"
#include "Exception/CCafException.h"
#include "CFileSystemUtils.h"
#include "../Collections/Iterators/IteratorsInc.h"
#ifdef WIN32
	#include <io.h>
#endif

#ifdef __linux__
#include <sys/sendfile.h>
#include <sys/stat.h>
#endif

#include <iostream>
#include <fstream>

using namespace Caf;

const std::string FileSystemUtils::REGEX_MATCH_ALL;

void FileSystemUtils::createDirectory(
	const std::string& path,
	const uint32 mode/* = 0770*/) {
	CAF_CM_STATIC_FUNC_LOG("FileSystemUtils", "createDirectory");

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STRING(path);

		if(doesDirectoryExist(path)) {
			CAF_CM_EXCEPTIONEX_VA1(
					IllegalStateException,
					ERROR_ALREADY_EXISTS,
					"Directory exists: %s",
					path.c_str());
		} else {
			CAF_CM_LOG_DEBUG_VA1("Creating directory - %s", path.c_str());
			int32 rc = g_mkdir_with_parents(path.c_str(), mode);
			if(rc < 0) {
				rc = errno;
				CAF_CM_EXCEPTIONEX_VA1(
						IOException,
						rc,
						"Unable to create directory: %s",
						path.c_str());
			}
		}
	}
	CAF_CM_EXIT;
}

void FileSystemUtils::removeDirectory(
	const std::string& path) {
	CAF_CM_STATIC_FUNC_LOG("FileSystemUtils", "removeDirectory");

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STRING(path);

		if(doesDirectoryExist(path)) {
			CAF_CM_LOG_DEBUG_VA1("Removing directory - %s", path.c_str());
			int32 rc = g_rmdir(path.c_str());
			if(rc < 0) {
				rc = errno;
				CAF_CM_EXCEPTIONEX_VA1(
						IOException,
						rc,
						"Failed to remove directory: %s",
						path.c_str());
			}
		} else {
			CAF_CM_EXCEPTIONEX_VA1(
					PathNotFoundException,
					0,
					"Directory does not exist: %s",
					path.c_str());
		}
	}
	CAF_CM_EXIT;
}

void FileSystemUtils::recursiveRemoveDirectory(const std::string& path) {
	CAF_CM_STATIC_FUNC("FileSystemUtils", "recursiveRemoveDirectory");

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STRING(path);

		if(doesDirectoryExist(path)) {
			DirectoryItems items = itemsInDirectory(path, REGEX_MATCH_ALL);

			// Delete subdirs first
			for (TConstIterator<Directories> directory(items.directories); directory; directory++) {
				recursiveRemoveDirectory(path + G_DIR_SEPARATOR_S + *directory);
			}

			// Delete files
			for (TConstIterator<Files> file(items.files); file; file++) {
				removeFile(path + G_DIR_SEPARATOR_S + *file);
			}

			// Delete directory
			removeDirectory(path);
		} else {
			CAF_CM_EXCEPTIONEX_VA1(
					PathNotFoundException,
					0,
					"Directory does not exist: %s",
					path.c_str());
		}
	}
	CAF_CM_EXIT;
}

void FileSystemUtils::removeFile(
	const std::string& path) {
	CAF_CM_STATIC_FUNC_LOG("FileSystemUtils", "removeFile");

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STRING(path);

		if(doesFileExist(path)) {
			CAF_CM_LOG_DEBUG_VA1("Removing file - %s", path.c_str());
			int32 rc = g_remove(path.c_str());
			if(rc < 0) {
				rc = errno;
				CAF_CM_EXCEPTIONEX_VA1(
						IOException,
						rc,"Failed to remove file: %s",
						path.c_str());
			}
		} else {
			CAF_CM_EXCEPTIONEX_VA1(
					FileNotFoundException,
					0,
					"File does not exist: %s",
					path.c_str());
		}
	}
	CAF_CM_EXIT;
}

FileSystemUtils::Files FileSystemUtils::removeFilesInDirectory(
		const std::string& path,
		const std::string& regex) {
	CAF_CM_STATIC_FUNC("FileSystemUtils", "removeFilesInDirectory");

	Files rc;

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STRING(path);

		if(doesDirectoryExist(path)) {
			DirectoryItems items = itemsInDirectory(path, regex);
			for (TConstIterator<Files> file(items.files); file; file++) {
				removeFile(path + G_DIR_SEPARATOR_S + *file);
				rc.push_back(*file);
			}
		} else {
			CAF_CM_EXCEPTIONEX_VA1(
					PathNotFoundException,
					0,
					"Directory does not exist: %s",
					path.c_str());
		}
	}
	CAF_CM_EXIT;

	return rc;
}

bool FileSystemUtils::doesFileExist(
	const std::string& path) {
	CAF_CM_STATIC_FUNC_VALIDATE( "FileSystemUtils", "doesFileExist" );
	CAF_CM_VALIDATE_STRING(path);

	return isRegularFile(path);
}

bool FileSystemUtils::doesDirectoryExist(
	const std::string& path) {

	CAF_CM_STATIC_FUNC_LOG_VALIDATE( "FileSystemUtils", "doesDirectoryExist" );

	bool rc = false;

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STRING(path);

		rc = (GLIB_TRUE == g_file_test(path.c_str(), G_FILE_TEST_IS_DIR));
	}
	CAF_CM_EXIT;

	return rc;
}

bool FileSystemUtils::isRegularFile(
	const std::string& path) {

	CAF_CM_STATIC_FUNC_VALIDATE( "FileSystemUtils", "isRegularFile" );

	bool rc = false;

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STRING(path);

		// Test to make sure the source is a regular file (or symlink to one), not a directory
		rc = (g_file_test(path.c_str(), G_FILE_TEST_IS_REGULAR) == GLIB_TRUE);
	}
	CAF_CM_EXIT;

	return rc;
}

std::string FileSystemUtils::getCurrentDir() {
	return getDirname(getCurrentFile());
}

std::string FileSystemUtils::getCurrentFile() {
	std::string fileName;
	CEcmDllManager::GetLibraryNameFromHandle(NULL, fileName);

	return fileName;
}

std::string FileSystemUtils::getBasename(
	const std::string& path) {

	CAF_CM_STATIC_FUNC( "FileSystemUtils", "getBasename" );

	std::string rc;

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STRING(path);

		gchar* basename = g_path_get_basename(path.c_str());
		if(NULL == basename) {
			CAF_CM_EXCEPTION_EFAIL( "g_path_get_basename Failed: " + path);
		}

		rc = basename;
		g_free(basename);
	}
	CAF_CM_EXIT;

	return rc;
}

std::string FileSystemUtils::getDirname(
	const std::string& path) {

	CAF_CM_STATIC_FUNC( "FileSystemUtils", "getDirname" );

	std::string rc;

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STRING(path);

		gchar* dirname = g_path_get_dirname(path.c_str());
		if(NULL == dirname) {
			CAF_CM_EXCEPTION_EFAIL( "g_path_get_dirname Failed: " + path);
		}

		rc = dirname;
		g_free(dirname);
	}
	CAF_CM_EXIT;

	return rc;
}

std::string FileSystemUtils::getTmpDir() {

	CAF_CM_STATIC_FUNC( "FileSystemUtils", "getTmpDir" );

	std::string rc;

	CAF_CM_ENTER {
		const gchar* tmpDir = g_get_tmp_dir();
		if(NULL == tmpDir) {
			CAF_CM_EXCEPTION_EFAIL( "g_get_tmp_dir Failed");
		} else {
			// Trim trailing slash
			rc = tmpDir;
			rc.erase(rc.find_last_not_of(G_DIR_SEPARATOR)+1);
		}
	}
	CAF_CM_EXIT;

	return rc;
}

std::string FileSystemUtils::buildPath(
	const std::string& path,
	const std::string& newElement) {

	CAF_CM_STATIC_FUNC( "FileSystemUtils", "buildPath" );

	std::string rc;

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STRING(path);
		CAF_CM_VALIDATE_STRING(newElement);

		gchar* newPath = g_build_filename(path.c_str(), newElement.c_str(), NULL);
		if(NULL == newPath) {
			CAF_CM_EXCEPTION_VA2(E_FAIL, "g_build_filename Failed: %s, %s", path.c_str(), newElement.c_str());
		}

		rc = normalizePathForPlatform(newPath);
		g_free(newPath);
	}
	CAF_CM_EXIT;

	return rc;
}

std::string FileSystemUtils::buildPath(
	const std::string& path,
	const std::string& newElement1,
	const std::string& newElement2) {

	CAF_CM_STATIC_FUNC( "FileSystemUtils", "buildPath" );

	std::string rc;

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STRING(path);
		CAF_CM_VALIDATE_STRING(newElement1);
		CAF_CM_VALIDATE_STRING(newElement2);

		gchar* newPath = g_build_filename(path.c_str(), newElement1.c_str(), newElement2.c_str(), NULL);
		if(NULL == newPath) {
			CAF_CM_EXCEPTION_VA3(E_FAIL, "g_build_filename Failed: %s, %s, %s", path.c_str(), newElement1.c_str(), newElement2.c_str());
		}

		rc = normalizePathForPlatform(newPath);
		g_free(newPath);
	}
	CAF_CM_EXIT;

	return rc;
}

std::string FileSystemUtils::buildPath(
	const std::string& path,
	const std::string& newElement1,
	const std::string& newElement2,
	const std::string& newElement3) {

	CAF_CM_STATIC_FUNC( "FileSystemUtils", "buildPath" );

	std::string rc;

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STRING(path);
		CAF_CM_VALIDATE_STRING(newElement1);
		CAF_CM_VALIDATE_STRING(newElement2);
		CAF_CM_VALIDATE_STRING(newElement3);

		gchar* newPath = g_build_filename(
			path.c_str(), newElement1.c_str(), newElement2.c_str(), newElement3.c_str(), NULL);
		if(NULL == newPath) {
			CAF_CM_EXCEPTION_VA4(E_FAIL,
				"g_build_filename Failed: %s, %s, %s, %s",
				path.c_str(), newElement1.c_str(), newElement2.c_str(), newElement3.c_str());
		}

		rc = normalizePathForPlatform(newPath);
		g_free(newPath);
	}
	CAF_CM_EXIT;

	return rc;
}

std::string FileSystemUtils::buildPath(
	const std::string& path,
	const std::string& newElement1,
	const std::string& newElement2,
	const std::string& newElement3,
	const std::string& newElement4) {

	CAF_CM_STATIC_FUNC( "FileSystemUtils", "buildPath" );

	std::string rc;

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STRING(path);
		CAF_CM_VALIDATE_STRING(newElement1);
		CAF_CM_VALIDATE_STRING(newElement2);
		CAF_CM_VALIDATE_STRING(newElement3);
		CAF_CM_VALIDATE_STRING(newElement4);

		gchar* newPath = g_build_filename(
			path.c_str(), newElement1.c_str(), newElement2.c_str(), newElement3.c_str(), newElement4.c_str(), NULL);
		if(NULL == newPath) {
			CAF_CM_EXCEPTION_VA5(E_FAIL,
				"g_build_filename Failed: %s, %s, %s, %s, %s",
				path.c_str(), newElement1.c_str(), newElement2.c_str(), newElement3.c_str(), newElement4.c_str());
		}

		rc = normalizePathForPlatform(newPath);
		g_free(newPath);
	}
	CAF_CM_EXIT;

	return rc;
}

std::string FileSystemUtils::loadTextFile(
	const std::string& path) {
	CAF_CM_STATIC_FUNC( "FileSystemUtils", "loadTextFile" );

	std::string rc;

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STRING(path);

		// Throw an exception if the file doesn't exist.
		if (!doesFileExist(path)) {
			CAF_CM_EXCEPTIONEX_VA1(
					FileNotFoundException,
					0,
					"The file '%s' does not exist.",
					path.c_str());
		}

		// Read in the contents of the file. Since this is a text file, it's terminated with NULL
		// so we needn't pass the length.
		GError *gError = NULL;
		gchar *fileContents = NULL;
		const bool isSuccessful = g_file_get_contents(path.c_str(), &fileContents, NULL, &gError);

		// If the call wasn't successful, throw an exception with the error information.
		if(! isSuccessful) {
			CAF_CM_VALIDATE_PTR(gError);

			const std::string errorMessage = gError->message;
			const int32 errorCode = gError->code;

			g_error_free(gError);

			CAF_CM_EXCEPTIONEX_VA2(
					IOException,
					errorCode,
					"g_file_get_contents Failed: %s File: %s",
					errorMessage.c_str(),
					path.c_str());
		}

		if (fileContents && ::strlen(fileContents)) {
			rc = fileContents;
		}
		g_free(fileContents);
	}
	CAF_CM_EXIT;

	return rc;
}

Cdeqstr FileSystemUtils::loadTextFileIntoColl(
		const std::string& path) {
	CAF_CM_STATIC_FUNC("FileSystemUtils", "loadTextFileIntoColl");
	CAF_CM_VALIDATE_STRING(path);

	// Throw an exception if the file doesn't exist.
	if (!doesFileExist(path)) {
		CAF_CM_EXCEPTIONEX_VA1(
				FileNotFoundException,
				0,
				"The file '%s' does not exist.",
				path.c_str());
	}

	std::ifstream fileStream(path.c_str());
	if (! fileStream.is_open()) {
		CAF_CM_EXCEPTION_VA1(E_UNEXPECTED,
				"Error opening file - %s", path.c_str());
	}

	Cdeqstr rc;
	std::string line;
	while(std::getline(fileStream, line)) {
		rc.push_back(line);
	}

	if (fileStream.bad()) {
		CAF_CM_EXCEPTION_VA1(E_UNEXPECTED,
				"Error reading file - %s", path.c_str());
	}

	return rc;
}

SmartPtrCDynamicByteArray FileSystemUtils::loadByteFile(
	const std::string& path) {
	CAF_CM_STATIC_FUNC("FileSystemUtils", "loadByteFile");

	SmartPtrCDynamicByteArray rc;

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STRING(path);

		// Throw an exception if the file doesn't exist.
		if (!doesFileExist(path)) {
			CAF_CM_EXCEPTIONEX_VA1(
					FileNotFoundException,
					0,
					"The file '%s' does not exist.",
					path.c_str());
		}

		// Read in the contents of the file. Since this is a text file, it's terminated with NULL
		// so we needn't pass the length.
		GError *gError = NULL;
		gchar *fileContents = NULL;
		gsize fileContentsLen = 0;
		const bool isSuccessful = g_file_get_contents(
				path.c_str(), &fileContents, &fileContentsLen, &gError);

		// If the call wasn't successful, throw an exception with the error information.
		if(! isSuccessful) {
			CAF_CM_VALIDATE_PTR(gError);

			const std::string errorMessage = gError->message;
			const int32 errorCode = gError->code;

			g_error_free(gError);

			CAF_CM_EXCEPTIONEX_VA2(
					IOException,
					errorCode,
					"g_file_get_contents Failed: %s File: %s",
					errorMessage.c_str(),
					path.c_str());
		}

		if (fileContents && fileContentsLen) {
			rc.CreateInstance();
			rc->allocateBytes(static_cast<uint32>(fileContentsLen));
			rc->memCpy(fileContents, static_cast<uint32>(fileContentsLen));
		}
		g_free(fileContents);
	}
	CAF_CM_EXIT;

	return rc;
}

void FileSystemUtils::saveTextFile(
	const std::string& outputDir,
	const std::string& filename,
	const std::string& contents,
	const FILE_MODE_TYPE fileMode/* = FILE_MODE_REPLACE*/,
	const std::string temporaryFileSuffix/* = ".tmp"*/) {
	CAF_CM_STATIC_FUNC_VALIDATE("FileSystemUtils", "saveTextFile");

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STRING(outputDir);
		CAF_CM_VALIDATE_STRING(filename);
		CAF_CM_VALIDATE_STRING(contents);

		const std::string filePath = buildPath(outputDir, filename);
		saveTextFile(filePath, contents, fileMode, temporaryFileSuffix);
	}
	CAF_CM_EXIT;
}

void FileSystemUtils::saveTextFile(
	const std::string& filePath,
	const std::string& contents,
	const FILE_MODE_TYPE fileMode/* = FILE_MODE_REPLACE*/,
	const std::string temporaryFileSuffix/* = ".tmp"*/) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("FileSystemUtils", "saveTextFile");

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STRING(filePath);
		CAF_CM_VALIDATE_STRING(contents);

		saveByteFile(filePath, reinterpret_cast<const byte*>(contents.c_str()),
			contents.length(), fileMode, temporaryFileSuffix);
	}
	CAF_CM_EXIT;
}

void FileSystemUtils::saveByteFile(
	const std::string& filePath,
	const SmartPtrCDynamicByteArray& contents,
	const FILE_MODE_TYPE fileMode/* = FILE_MODE_REPLACE*/,
	const std::string temporaryFileSuffix/* = ".tmp"*/) {
	CAF_CM_STATIC_FUNC_VALIDATE("FileSystemUtils", "saveByteFile");
	CAF_CM_VALIDATE_STRING(filePath);
	CAF_CM_VALIDATE_SMARTPTR(contents);
	// temporaryFileSuffix is optional

	saveByteFile(filePath, contents->getPtr(), contents->getByteCount(),
			fileMode, temporaryFileSuffix);
}

void FileSystemUtils::saveByteFile(
	const std::string& filePath,
	const byte* contents,
	const size_t& contentsLen,
	const FILE_MODE_TYPE fileMode/* = FILE_MODE_REPLACE*/,
	const std::string temporaryFileSuffix/* = ".tmp"*/) {
	CAF_CM_STATIC_FUNC_LOG("FileSystemUtils", "saveByteFile");
	CAF_CM_VALIDATE_STRING(filePath);
	CAF_CM_VALIDATE_PTR(contents);
	// temporaryFileSuffix is optional

	const std::string fileDir = getDirname(filePath);
	if (! doesDirectoryExist(fileDir)) {
		createDirectory(fileDir);
	}

	bool fileExists = doesFileExist(filePath);
	switch (fileMode) {
		case FILE_MODE_REPLACE:
			if (fileExists) {
				CAF_CM_LOG_DEBUG_VA1("Replacing file - %s", filePath.c_str());
			}
			saveFileSafely(filePath, contents, contentsLen, temporaryFileSuffix);
		break;
		case FILE_MODE_IGNORE:
			if (fileExists) {
				CAF_CM_LOG_WARN_VA1("Ignoring file - %s", filePath.c_str());
			} else {
				saveFileSafely(filePath, contents, contentsLen, temporaryFileSuffix);
			}
		break;
		case FILE_MODE_FAIL:
			if (fileExists) {
				CAF_CM_EXCEPTION_VA1(ERROR_FILE_EXISTS, "File exists - %s",
					filePath.c_str());
			}
			saveFileSafely(filePath, contents, contentsLen, temporaryFileSuffix);
		break;
	}
}

void FileSystemUtils::saveFileSafely(
	const std::string& filePath,
	const byte* contents,
	const size_t& contentsLen,
	const std::string& temporaryFileSuffix) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("FileSystemUtils", "saveFileSafely");
	CAF_CM_VALIDATE_STRING(filePath);
	CAF_CM_VALIDATE_PTR(contents);
	// temporaryFileSuffix is optional

	CAF_CM_LOG_DEBUG_VA1("Saving to file - %s", filePath.c_str());

	if (temporaryFileSuffix.empty()) {
		saveByteFileRaw(filePath, contents, contentsLen);
	} else {
		const std::string filePathTmp = filePath + temporaryFileSuffix;
		saveByteFileRaw(filePathTmp, contents, contentsLen);
		moveFile(filePathTmp, filePath);
	}
}

void FileSystemUtils::saveByteFileRaw(
	const std::string& path,
	const byte* contents,
	const size_t& contentsLen) {
	CAF_CM_STATIC_FUNC_LOG("FileSystemUtils", "saveByteFileRaw");
	CAF_CM_VALIDATE_STRING(path);
	CAF_CM_VALIDATE_PTR(contents);

	GError *gError = NULL;
	const bool isSuccessful = g_file_set_contents(
		path.c_str(), reinterpret_cast<const char*>(contents), contentsLen, &gError);

	// If the call wasn't successful, throw an exception with the error information.
	if(! isSuccessful) {
		CAF_CM_VALIDATE_PTR(gError);

		const std::string errorMessage = gError->message;
		const int32 errorCode = gError->code;

		g_error_free(gError);

		CAF_CM_EXCEPTIONEX_VA2(
				IOException,
				errorCode,
				"g_file_set_contents Failed: %s File: %s",
				errorMessage.c_str(),
				path.c_str());
	}
}

FileSystemUtils::DirectoryItems FileSystemUtils::itemsInDirectory(
	const std::string& path,
	const std::string& regex) {

	CAF_CM_STATIC_FUNC( "FileSystemUtils", "itemsInDirectory" );

	DirectoryItems rc;
	GDir *gDir = NULL;
	GError *gError = NULL;
	GRegex *gRegex = NULL;

	try {
		CAF_CM_VALIDATE_STRING(path);

		if(! doesDirectoryExist(path)) {
			CAF_CM_EXCEPTIONEX_VA1(
					PathNotFoundException,
					0,
					"Directory does not exist: %s",
					path.c_str());
		}

		gDir = g_dir_open(path.c_str(), 0, &gError);
		if(NULL == gDir) {
			const std::string errorMessage = (gError == NULL) ? "" : gError->message;
			const int32 errorCode = (gError == NULL) ? 0 : gError->code;

			CAF_CM_EXCEPTIONEX_VA2(
					IOException,
					errorCode,
					"Failed to open directory \"%s\": %s",
					path.c_str(),
					errorMessage.c_str());
		}

		if(regex.compare(REGEX_MATCH_ALL) != 0) {
			gRegex = g_regex_new(regex.c_str(),
								 (GRegexCompileFlags)(G_REGEX_OPTIMIZE | G_REGEX_RAW),
								 (GRegexMatchFlags)0,
								 &gError);
			if(gError) {
				const std::string errorMessage = gError->message;
				const int32 errorCode = gError->code;

				CAF_CM_EXCEPTIONEX_VA2(IOException, errorCode,
					"g_regex_new Failed: %s regex: %s",
					errorMessage.c_str(),
					regex.c_str());
			}
		}

		for(const gchar* filename = g_dir_read_name(gDir);
			filename != NULL;
			filename = g_dir_read_name(gDir)) {
			std::string fullPath(path);
			fullPath += G_DIR_SEPARATOR_S;
			fullPath += filename;
			bool isDirectory = g_file_test(fullPath.c_str(), G_FILE_TEST_IS_DIR);

			if (gRegex) {
				if (g_regex_match(gRegex,
								  filename,
								  (GRegexMatchFlags)0,
								  NULL)) {
					if (isDirectory) {
						rc.directories.push_back(filename);
					} else {
						rc.files.push_back(filename);
					}
				}
			} else {
				if (isDirectory) {
					rc.directories.push_back(filename);
				} else {
					rc.files.push_back(filename);
				}
			}
		}

		if(gError != NULL) {
			g_error_free(gError);
		}
		if(gDir != NULL) {
			g_dir_close(gDir);
		}
		if(gRegex != NULL) {
			g_regex_unref(gRegex);
		}
	}
	catch(...) {
		if(gError != NULL) {
			g_error_free(gError);
		}
		if(gDir != NULL) {
			g_dir_close(gDir);
		}
		if(gRegex != NULL) {
			g_regex_unref(gRegex);
		}
		throw;
	}

	return rc;
}

FileSystemUtils::PathAndDirectoryItemsCollection
FileSystemUtils::recursiveItemsInDirectory(const std::string& path, const std::string& regex) {
	PathAndDirectoryItemsCollection rc;

	CAF_CM_ENTER {
		DirectoryItems items = itemsInDirectory(path, regex);
		rc.push_back(PathAndDirectoryItems(path, items));

		for (TConstIterator<Directories> subdir(items.directories); subdir; subdir++) {
			std::string subdirPath = buildPath(path, *subdir);
			PathAndDirectoryItemsCollection subitems = recursiveItemsInDirectory(subdirPath, regex);
			rc.insert(rc.end(), subitems.begin(), subitems.end());
		}
	}
	CAF_CM_EXIT;

	return rc;
}

void FileSystemUtils::copyFile(const std::string& srcPath, const std::string& dstPath) {
	CAF_CM_STATIC_FUNC_LOG("FileSystemUtils", "copyFile");

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STRING(srcPath);
		CAF_CM_VALIDATE_STRING(dstPath);

		// Test to make sure the source is a regular file (or symlink to one), not a directory
		if (! isRegularFile(srcPath)) {
			CAF_CM_EXCEPTIONEX_VA1(
					UnsupportedOperationException,
					0,
					"Source is not a regular file: %s",
					srcPath.c_str());
		}

		const std::string dstDir = getDirname(dstPath);
		if (!doesDirectoryExist(dstDir)) {
			CAF_CM_EXCEPTIONEX_VA1(
					PathNotFoundException,
					0,
					"Destination path does not exist: %s",
					dstDir.c_str());
		}

#if defined (__linux__) || defined (__APPLE__)
		int32 infd = -1;
		int32 outfd = -1;
		try {
			// Determine the stats of the source file, for use with output file later
			struct stat srcStat;
			if (::stat(srcPath.c_str(), &srcStat) != 0) {
				int32 rc = errno;
				CAF_CM_EXCEPTIONEX_VA1(
						IOException,
						rc,
						"::stat %s failed",
						srcPath.c_str());
			}

			infd = ::open(srcPath.c_str(), O_RDONLY);
			if (infd == -1) {
				int32 rc = errno;
				CAF_CM_EXCEPTIONEX_VA1(
						IOException,
						rc,
						"Unable to open %s for reading",
						srcPath.c_str());
			}

			// Open output file write-only for user, initially, and set to source file's mode bits later
			outfd = ::open(dstPath.c_str(), O_WRONLY | O_CREAT, S_IWUSR);
			if (outfd == -1) {
				int32 rc = errno;
				CAF_CM_EXCEPTIONEX_VA1(
						IOException,
						rc,
						"Unable to open %s for writing",
						dstPath.c_str());
			}

			// Arbitrary size for the data buffer to use for the copy
			const int32 buff_size = 65536;

			byte buffer[buff_size];
			ssize_t index = 0;
			// Loop until we've processed the whole file
			while (index < srcStat.st_size) {
				ssize_t bytesRead = ::read(infd, buffer, buff_size);
				if (bytesRead > 0) {
					ssize_t writeIndex = 0;
					// Loop until we've written out all the buffer
					while (writeIndex < bytesRead) {
						// If we're part way done, write from where we left off, but with a smaller number of bytes left
						ssize_t bytesWritten = ::write(outfd, buffer + writeIndex, bytesRead - writeIndex);
						if (bytesWritten < 0) {
							int32 rc = errno;
							CAF_CM_EXCEPTIONEX_VA2(
									IOException,
									rc,
									"Unable to write to %s (index = %d)",
									dstPath.c_str(),
									writeIndex);
						}
						writeIndex += bytesWritten;
					}
					index += bytesRead;
				} else if (bytesRead < 0) {
					int32 rc = errno;
					CAF_CM_EXCEPTIONEX_VA2(
							IOException,
							rc,
							"Unable to read from %s at offset %d",
							srcPath.c_str(),
							index);
				}

			}
			if (outfd != -1) {
				// Lastly, update the mode bits on the destination file to be the same as the source
				::fchmod(outfd, srcStat.st_mode);
			}
		}
		CAF_CM_CATCH_ALL;

		if (infd != -1) {
			::close(infd);
		}
		if (outfd != -1) {
			::close(outfd);
		}
		CAF_CM_THROWEXCEPTION;
#elif defined(WIN32)
	const std::wstring wSrcPath = CStringUtils::convertNarrowToWide(srcPath);
	const std::wstring wDstPath = CStringUtils::convertNarrowToWide(dstPath);
	BOOL bRc = ::CopyFileW(
					wSrcPath.c_str(),
					wDstPath.c_str(),
					TRUE);
	if (!bRc) {
		const DWORD lastError = ::GetLastError();
		const std::string errorMsg = BasePlatform::PlatformApi::GetApiErrorMessage(lastError);
		CAF_CM_EXCEPTIONEX_VA3(
			IOException,
			lastError,
			"Failed to copy file %s to %s - %s",
			srcPath.c_str(),
			dstPath.c_str(),
			errorMsg.c_str());
	}
#else
#error not implemented
#endif
	}
	CAF_CM_EXIT;
}

void FileSystemUtils::moveFile(const std::string& srcPath, const std::string& dstPath) {
	CAF_CM_STATIC_FUNC("FileSystemUtils", "moveFile");

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STRING(srcPath);
		CAF_CM_VALIDATE_STRING(dstPath);

		if (!doesFileExist(srcPath)) {
			CAF_CM_EXCEPTIONEX_VA1(
					FileNotFoundException,
					0,
					"Source file does not exist: %s",
					srcPath.c_str());
		}
		if (!doesDirectoryExist(getDirname(dstPath))) {
			createDirectory(getDirname(dstPath));
		}

		if (g_rename(srcPath.c_str(), dstPath.c_str()) == -1) {
			int32 rc = errno;
			CAF_CM_EXCEPTIONEX_VA2(
					IOException,
					rc,
					"Unable to move file %s to %s",
					srcPath.c_str(),
					dstPath.c_str());
		}
	}
	CAF_CM_EXIT;
}

void FileSystemUtils::copyDirectory(const std::string& srcPath, const std::string& dstPath) {
	CAF_CM_STATIC_FUNC("FileSystemUtils", "copyDirectory");

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STRING(srcPath);
		CAF_CM_VALIDATE_STRING(dstPath);

		if(doesDirectoryExist(dstPath)) {
			CAF_CM_EXCEPTIONEX_VA1(
					IOException,
					ERROR_ALREADY_EXISTS,
					"Cannot copy into an existing directory: %s",
					dstPath.c_str());
		}
		createDirectory(dstPath);

		if(!doesDirectoryExist(srcPath)) {
			CAF_CM_EXCEPTIONEX_VA1(
					PathNotFoundException,
					0,
					"Invalid source directory: %s",
					srcPath.c_str());
		}

		const DirectoryItems items = itemsInDirectory(srcPath, FileSystemUtils::REGEX_MATCH_ALL);
		for (TConstIterator<Files> srcFile(items.files); srcFile; srcFile++) {
			copyFile(srcPath + G_DIR_SEPARATOR_S + *srcFile,
					 dstPath + G_DIR_SEPARATOR_S + *srcFile);
		}
	}
	CAF_CM_EXIT;
}

void FileSystemUtils::recursiveCopyDirectory(const std::string& srcPath, const std::string& dstPath) {
	CAF_CM_STATIC_FUNC_VALIDATE("FileSystemUtils", "recursiveCopyDirectory");

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STRING(srcPath);
		CAF_CM_VALIDATE_STRING(dstPath);

		const DirectoryItems items = itemsInDirectory(srcPath, FileSystemUtils::REGEX_MATCH_ALL);

		// Copy subdirectories first
		for (TConstIterator<Directories> srcDir(items.directories); srcDir; srcDir++) {
			copyDirectory(srcPath + G_DIR_SEPARATOR_S + *srcDir,
						  dstPath + G_DIR_SEPARATOR_S + *srcDir);
		}

		// Copy files second
		for (TConstIterator<Files> srcFile(items.files); srcFile; srcFile++) {
			copyFile(srcPath + G_DIR_SEPARATOR_S + *srcFile,
					 dstPath + G_DIR_SEPARATOR_S + *srcFile);
		}
	}
	CAF_CM_EXIT;
}

std::string FileSystemUtils::findOptionalFile(
	const std::string& directory,
	const std::string& filename) {

	CAF_CM_STATIC_FUNC_LOG("FileSystemUtils", "findOptionalFile");

	std::string rc;

	CAF_CM_ENTER
	{
		CAF_CM_VALIDATE_STRING(directory);
		CAF_CM_VALIDATE_STRING(filename);

		const std::deque<std::string> files = findOptionalFiles(directory, filename);
		if (files.size() == 1) {
			rc = files[0];
		} else if (files.size() > 1) {
			CAF_CM_EXCEPTION_VA2(ERROR_FILE_EXISTS,
				"Found more than one file - directory: %s, filename: %s",
				directory.c_str(), filename.c_str())
		}
	}
	CAF_CM_EXIT;

	return rc;
}

std::string FileSystemUtils::findRequiredFile(
	const std::string& directory,
	const std::string& filename) {

	CAF_CM_STATIC_FUNC_LOG("FileSystemUtils", "findRequiredFile");

	std::string rc;

	CAF_CM_ENTER
	{
		CAF_CM_VALIDATE_STRING(directory);
		CAF_CM_VALIDATE_STRING(filename);

		rc = findOptionalFile(directory, filename);
		if (rc.empty()) {
			CAF_CM_EXCEPTION_VA2(ERROR_FILE_NOT_FOUND,
				"File not found - directory: %s, filename: %s",
				directory.c_str(), filename.c_str())
		}
	}
	CAF_CM_EXIT;

	return rc;
}

std::deque<std::string> FileSystemUtils::findOptionalFiles(
	const std::string& directory,
	const std::string& filename) {

	CAF_CM_STATIC_FUNC_LOG_VALIDATE("FileSystemUtils", "findOptionalFiles");

	std::deque<std::string> rc;

	CAF_CM_ENTER
	{
		CAF_CM_VALIDATE_STRING(directory);
		CAF_CM_VALIDATE_STRING(filename);

		PathAndDirectoryItemsCollection pAndDItemsCol =
			recursiveItemsInDirectory(directory, REGEX_MATCH_ALL);
		for (TConstIterator<PathAndDirectoryItemsCollection>
			pAndDItemsColIter(pAndDItemsCol); pAndDItemsColIter; pAndDItemsColIter++) {
			const std::string path = pAndDItemsColIter->path;
			const DirectoryItems directoryItems =
				pAndDItemsColIter->items;
			const Files files = directoryItems.files;
			for (TConstIterator<Files> fileIter(files); fileIter; fileIter++) {
				const std::string filenameCur = *fileIter;
				if (filenameCur.compare(filename) == 0) {
					const std::string filePath = buildPath(path, filename);
					rc.push_back(filePath);
				}
			}
		}
	}
	CAF_CM_EXIT;

	return rc;
}

std::deque<std::string> FileSystemUtils::findRequiredFiles(
	const std::string& directory,
	const std::string& filename) {

	CAF_CM_STATIC_FUNC_LOG("FileSystemUtils", "findRequiredFiles");

	std::deque<std::string> rc;

	CAF_CM_ENTER
	{
		CAF_CM_VALIDATE_STRING(directory);
		CAF_CM_VALIDATE_STRING(filename);

		rc = findOptionalFiles(directory, filename);
		if (rc.empty()) {
			CAF_CM_EXCEPTION_VA2(ERROR_FILE_NOT_FOUND,
				"File not found - directory: %s, filename: %s",
				directory.c_str(), filename.c_str())
		}
	}
	CAF_CM_EXIT;

	return rc;
}

void FileSystemUtils::chmod(
	const std::string &path,
	const uint32 mode/* = 0770*/) {

	CAF_CM_STATIC_FUNC_LOG("FileSystemUtils", "chmod");

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STRING(path);

		if(! doesFileExist(path)) {
			CAF_CM_EXCEPTIONEX_VA1(FileNotFoundException, 0,
				"File does not exist: %s", path.c_str());
		}

		// Not using g_chmod on Nix because it hangs.
#ifdef WIN32
		g_chmod(path.c_str(), mode);
#else
		const int32 rc = ::chmod(path.c_str(), mode);
		if (rc != 0) {
			CAF_CM_EXCEPTION_VA1(E_INVALIDARG,
				"chmod failed - file: %s, filename: %s", path.c_str());
		}
#endif
	}
	CAF_CM_EXIT;
}

std::string FileSystemUtils::normalizePathForPlatform(
	const std::string &path) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("FileSystemUtils", "normalizePathForPlatform");

	std::string rc;

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STRING(path);

		rc = path;
#ifdef WIN32
		std::replace(rc.begin(), rc.end(), '/', '\\');
#else
		std::replace(rc.begin(), rc.end(), '\\', '/');
#endif
	}
	CAF_CM_EXIT;

	return rc;
}

std::string FileSystemUtils::normalizePathWithForward(
	const std::string &path) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("FileSystemUtils", "normalizePathWithForward");

	std::string rc;

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STRING(path);

		rc = path;
		std::replace(rc.begin(), rc.end(), '\\', '/');
	}
	CAF_CM_EXIT;

	return rc;
}

int64 FileSystemUtils::getFileSize(const std::string& filename) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("FileSystemUtils", "getFileSize");
	CAF_CM_VALIDATE_STRING(filename);

	struct stat stat_buf;
	int32 rc = ::stat(filename.c_str(), &stat_buf);
	return rc == 0 ? stat_buf.st_size : -1;
}

std::string FileSystemUtils::saveTempTextFile(const std::string& filename_template, const std::string& contents) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("FileSystemUtils", "saveTempTextFile");

	std::string filename;
	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STRING(filename_template);
		CAF_CM_VALIDATE_STRING(contents);

		filename = FileSystemUtils::getTempFilename(filename_template);

		FileSystemUtils::saveTextFile(filename, contents);
	}
	CAF_CM_EXIT;
	
	return filename;
}

std::string FileSystemUtils::getTempFilename(const std::string& filename_template) {
	CAF_CM_STATIC_FUNC("FileSystemUtils", "getTempFilename");

	std::string filename;

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STRING(filename_template);

		gchar* allocatedFilename;
		GError *gError = NULL;

		int32 fd = g_file_open_tmp(filename_template.c_str(), &allocatedFilename, &gError);
		if (fd >= 0) {
			filename = allocatedFilename;
#ifdef WIN32
			::_close(fd);
#else
			::close(fd);
#endif
			g_free(allocatedFilename);
		}
		if (gError != NULL) {
			const std::string errorMessage = gError->message;
			const int32 errorCode = gError->code;

			g_error_free(gError);

			CAF_CM_EXCEPTIONEX_VA2(
					IOException,
					errorCode,
					"g_file_open_tmp Failed: %s Template: %s",
					errorMessage.c_str(),
					filename_template.c_str());
		}
	}
	CAF_CM_EXIT;
	CAF_CM_VALIDATE_STRING(filename);
	
	return filename;
}
