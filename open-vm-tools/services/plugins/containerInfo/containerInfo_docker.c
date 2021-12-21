/*********************************************************
 * Copyright (C) 2021 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation version 2.1 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the Lesser GNU General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA.
 *
 *********************************************************/

/*
 * containerInfo_docker.c --
 *
 *    This file defines docker specific functions which are needed by
 *    containerInfo. Docker API is called using libcurl to find runnning
 *    docker containers and collect relevant info.
 */

#include <stdio.h>
#include <stdlib.h>
#include "containerInfoInt.h"
#include "jsmn.h"
#include <curl/curl.h>
#include "vm_assert.h"

#define HTTP_HEADER "HTTP"
#define HTTP_HEADER_LENGTH (sizeof HTTP_HEADER - 1)
#define HTTP_STATUS_SUCCESS "200"
#define HTTP_STATUS_SUCCESS_LENGTH (sizeof HTTP_STATUS_SUCCESS - 1)
#define TOKENS_PER_ALLOC 500
#define MAX_TOKENS 100000

/*
 * docker API versions are backwards compatible with older docker Engine
 * versions so this is the oldest API version that is documented by docker
 * at https://docs.docker.com/engine/api/
 */
#define DOCKER_API_VERSION "v1.18"

typedef struct DockerBuffer {
  char *response;
  size_t size;
} DockerBuffer;


/*
 ******************************************************************************
 * ContainerInfoJsonEq --
 *
 * @brief Utility function to check whether a string jsmn token has value
 * equal to @param s
 *
 * @param[in] json The json string
 * @param[in] tok The jsmn token structure pointer
 * @param[in] s The string to match in the token
 *
 * @retval TRUE successfully matched the string in the json token
 * @retval FALSE did not match the string in the json token
 *
 ******************************************************************************
 */

static gboolean
ContainerInfoJsonEq(const char *json,
                    jsmntok_t *tok,
                    const char *s)
{
   if (tok->type == JSMN_STRING &&
       (int) strlen(s) == tok->end - tok->start &&
       tok->start >= 0 && tok->end < strlen(json) &&
       strncmp(json + tok->start, s, tok->end - tok->start) == 0) {
      return TRUE;
   }
   return FALSE;
}


/*
 ******************************************************************************
 * ContainerInfoJsonEqIsKey --
 *
 * @brief Utility function that is same as ContainerInfoJsonEq() but also
 *        checks token is of the key type in the json.
 *
 * @param[in] json The json string
 * @param[in] tok The jsmn token structure pointer
 * @param[in] s The string to match in the token
 *
 * @retval TRUE successfully matched the string and token is a key
 * @retval FALSE did not match the string or token is not a key
 *
 ******************************************************************************
 */

static gboolean
ContainerInfoJsonEqIsKey(const char *json,
                         jsmntok_t *tok,
                         const char *s)
{
   /*
    * Check tok->size. If tok is a key, tok->size will be 1.
    */
   return tok->size == 1 && ContainerInfoJsonEq(json, tok, s);;
}


/*
 ******************************************************************************
 * DockerWriteCB --
 *
 * @brief Sets callback for writing received data when using libcurl to access
 *        docker API. This function prototype is based on
 *        https://curl.se/libcurl/c/CURLOPT_WRITEFUNCTION.html
 *
 * @param[in] data       info received from API
 * @param[in] size       this value is always 1 (according to curl docs)
 * @param[in] nitems     size of data
 * @param[in] userdata   pointer to DockerBuffer
 *
 * @retval   number of bytes successfully written
 *
 ******************************************************************************
 */

static size_t
DockerWriteCB(void *data,                    // IN
              size_t size,                   // IN
              size_t nitems,                 // IN
              void *userdata)                // IN
{
   size_t realsize = size * nitems;
   DockerBuffer *mem = (DockerBuffer *) userdata;
   char *realptr;
   size_t newsize = mem->size + realsize + 1;

   if (newsize < mem->size) {
      g_warning("%s:%d: size overflow\n", __FUNCTION__, __LINE__);
      g_free(mem->response);
      mem->response = NULL;
      return 0;
   }

   realptr = g_try_realloc(mem->response, newsize);
   if (realptr == NULL) {
      g_warning("%s:%d: out of memory\n", __FUNCTION__, __LINE__);
      g_free(mem->response);
      mem->response = NULL;
      return 0;
   }

   mem->response = realptr;
   memcpy(&mem->response[mem->size], data, realsize);
   mem->size += realsize;
   mem->response[mem->size] = '\0';

   return realsize;
}


/*
 ******************************************************************************
 * DockerHeaderCB --
 *
 * @brief Sets callback for receiving header data and saving HTTP status
 *        when using libcurl to access docker API. For more info see
 *        https://curl.se/libcurl/c/CURLOPT_HEADERFUNCTION.html
 *
 * @param[in] buffer     info received from API
 * @param[in] size       this value is always 1 (according to curl docs)
 * @param[in] nitems     size of buffer
 * @param[in] userdata   pointer to string to store docker status code
 *
 * @retval   number of bytes of header data successfully received
 *
 ******************************************************************************
 */

static size_t
DockerHeaderCB(char *buffer,                     // IN
               size_t size,                      // IN
               size_t nitems,                    // IN
               void *userdata)                   // IN
{
   size_t realSize = size * nitems;
   char **statusCode = (char **) userdata;
   char *statusStart;
   char *statusEnd;
   char *bufPtr;
   char *bufEnd;

   /*
    * Example of buffer: HTTP/1.1 404 Not Found\r\n
    * Do not assume that buffer is null-terminated!
    */
   if (realSize <= HTTP_HEADER_LENGTH ||
       memcmp(buffer, HTTP_HEADER, HTTP_HEADER_LENGTH) != 0) {
      /*
       * This is a separated header line, like: Api-Version: 1.41
       */
      return realSize;
   }

   bufEnd = buffer + realSize;
   bufPtr = buffer + HTTP_HEADER_LENGTH;
   statusStart = memchr(bufPtr, ' ', bufEnd - bufPtr);

   if (statusStart == NULL) {
      g_debug("%s:%d: HTTP header has unexpected format: %.*s\n",
              __FUNCTION__, __LINE__, (int) realSize, buffer);
      return 0;
   }

   bufPtr = ++statusStart;
   statusEnd = memchr(bufPtr, ' ', bufEnd - bufPtr);

   if (statusEnd == NULL) {
      g_debug("%s:%d: HTTP header has unexpected format: %.*s\n",
              __FUNCTION__, __LINE__, (int) realSize, buffer);
      return 0;
   }

   *statusCode = g_strndup(statusStart, statusEnd - statusStart);
   return realSize;
}


/*
 ******************************************************************************
 * DockerCallAPI --
 *
 * @brief Uses libcurl to access docker API and loads response to jsonString.
 *
 * @param[in] url              url of docker API endpoint.
 *                              e.g. http://v1.18/containers/json
 * @param[in] unixSocket       unix socket to communicate with docker.
 * @param[in/out] jsonString   stores the response from docker API.
 *
 * @retval TRUE   successfully wrote valid response to jsonString
 * @retval FALSE  on failure
 *
 ******************************************************************************
 */

static gboolean
DockerCallAPI(const char *url,                     // IN
              const char *unixSocket,              // IN
              char **jsonString)                   // OUT
{
   DockerBuffer result = {0};
   char *dockerStatus = NULL;
   CURLcode ret;
   char errBuf[CURL_ERROR_SIZE] = {'\0'};
   gboolean retVal = FALSE;
   CURL *curl = curl_easy_init();

   if (curl == NULL) {
      g_warning("%s:%d: curl failed to initialize\n",
                __FUNCTION__, __LINE__);
      return retVal;
   }

   curl_easy_setopt(curl, CURLOPT_UNIX_SOCKET_PATH, unixSocket);
   curl_easy_setopt(curl, CURLOPT_URL, url);
   curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errBuf);
   curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, DockerHeaderCB);
   curl_easy_setopt(curl, CURLOPT_HEADERDATA, &dockerStatus);
   curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, (void *) DockerWriteCB);
   curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) &result);

   ret = curl_easy_perform(curl);

   if (ret == CURLE_OK && result.size > 0) {
      /*
       * might receive CURLE_OK from libcurl but dockerStatus does not
       * equal 200.  e.g. when page is not found by docker engine.
       */
      if (dockerStatus != NULL &&
          strncmp(dockerStatus, HTTP_STATUS_SUCCESS,
                  HTTP_STATUS_SUCCESS_LENGTH) == 0) {
         *jsonString = result.response;
         retVal = TRUE;
      } else {
         g_warning("%s:%d: error response from docker engine. response: %s",
                   __FUNCTION__, __LINE__, result.response != NULL ?
                   result.response : "No response from docker engine.");
         g_free(result.response);
      }
   } else {
      if (errBuf[0] != '\0') {
         g_warning("%s:%d: %s\n", __FUNCTION__, __LINE__, errBuf);
      } else {
         g_warning("%s:%d: docker request unsuccessful. strerror: %s\n",
                   __FUNCTION__, __LINE__, curl_easy_strerror(ret));
      }
      g_free(result.response);
   }

   g_free(dockerStatus);
   curl_easy_cleanup(curl);
   return retVal;
}


/*
 ******************************************************************************
 * ContainerInfoParseString --
 *
 * @brief  parses and stores input string as tokens.
 *
 * @param[in] jsonString  The string to parse.
 * @param[in/out] tokens  Stores tokenized version of jsonString.
 *
 * @retval  number of tokens parsed from jsonString or -1 on failure.
 ******************************************************************************
 */

static int
ContainerInfoParseString(char *jsonString,            // IN
                         jsmntok_t **tokens)          // IN/OUT
{
   gsize jsonLength;
   jsmn_parser parser;
   int numTokens;
   int ret;

   ASSERT(jsonString);
   jsmn_init(&parser);
   jsonLength = strlen(jsonString);
   numTokens = TOKENS_PER_ALLOC;
   *tokens = (jsmntok_t *) g_malloc0(numTokens * sizeof(jsmntok_t));

   while ((ret = jsmn_parse(&parser, jsonString, jsonLength,
                            *tokens, numTokens)) == JSMN_ERROR_NOMEM) {
      numTokens += TOKENS_PER_ALLOC;
      if (numTokens > MAX_TOKENS) {
         g_warning("%s:%d: number of jsmn tokens: %d exceeded max :%d",
                   __FUNCTION__, __LINE__,
                   numTokens, MAX_TOKENS);
         g_free(*tokens);
         *tokens = NULL;
         return -1;
      }
      *tokens = g_realloc(*tokens, numTokens * sizeof(jsmntok_t));
   }

   if (ret < 0) {
      g_warning("%s:%d: jsmn error: %d parsing failed at character %d\n",
                __FUNCTION__, __LINE__, ret, parser.pos);
      g_free(*tokens);
      *tokens = NULL;
   }

   return ret;
}


/*
 *****************************************************************************
 * ContainerInfo_GetDockerContainers --
 *
 * @brief  Entry point for gathering running docker container info
 *
 *
 * @retval TRUE   successfully collected docker container info
 * @retval FALSE  on failure
 *
 *****************************************************************************
 */

GHashTable *
ContainerInfo_GetDockerContainers(const char *dockerSocketPath)          // IN
{
   jsmntok_t *t = NULL;
   int i;
   int numTokens;
   char *dockerContainerString = NULL;
   char *endpt = g_strdup_printf("http://%s/containers/json?"
                                 "filters={\"status\":[\"running\"]}",
                                 DOCKER_API_VERSION);
   GHashTable *containerTable = NULL;

   if (!DockerCallAPI(endpt,
                      dockerSocketPath,
                      &dockerContainerString)) {
       g_warning("%s: Failed to get the list of containers.", __FUNCTION__);
       goto exit;
   }

   numTokens = ContainerInfoParseString(dockerContainerString, &t);

   if (numTokens <= 0 || t[0].type != JSMN_ARRAY) {
      g_warning("%s: invalid json response\n",
                __FUNCTION__);
      goto exit;
   }

   containerTable = g_hash_table_new_full(g_str_hash, g_str_equal,
                                          g_free, g_free);

   /* Example of "GET containers/json" response.
    * Each item in the array is a running container.
    * [{"Id":"370a480816ec5207c620fe628bd162925b85d150b3303601f76c3fe47ed863de",
    *   "Names":["/fervent_goldwasser"],
    *   "Image":"redis",
    *   "ImageID":"sha256:de974760ddb2f32dbddb74b7bb8cff4c1eee06d43d36d11bbc",
    *   "Command":"docker-entrypoint.sh redis-server",
    *   "Created":1623742538,
    *   "Ports":[{"PrivatePort":6379,"Type":"tcp"}],
    *   "Labels":{},
    *   "State":"running",
    *   "Status":"Up 29 minutes",
    *   "HostConfig":{"NetworkMode":"default"},
    *   "NetworkSettings":{...},
    *   "Mounts":[...]},
    *  {"Id":"b3ba5ed8b84816c66a6b6fe5903565164ea953ecdddf190263d52ed6ad0f6088",
    *   "Names":["/bold_solomon"],
    *   "Image":"nginx",
    *   "ImageID":"sha256:62d49f9bab67f7c70ac3395855bf01389eb3175b374e621f6f19",
    *   "Command":"/docker-entrypoint.sh nginx -g 'daemon off;'",
    *   "Created":1623742533,
    *   "Ports":[{"PrivatePort":80,"Type":"tcp"}],
    *   "Labels":{"maintainer":"NGINX Docker Maintainers"},
    *   "State":"running",
    *   "Status":"Up 29 minutes",
    *   "HostConfig":{"NetworkMode":"default"},
    *   "NetworkSettings":{...},
    *   "Mounts":[]}]
    */
   i = 1;
   while (i < numTokens) {
      if (t[i].type == JSMN_OBJECT) {
         char *id = NULL;
         char *image = NULL;
         int end = t[i].end;

         i++;
         while (i < numTokens - 1 && t[i + 1].start < end) {
            if (t[i].type == JSMN_STRING &&
                t[i + 1].type == JSMN_STRING) {
               if (ContainerInfoJsonEqIsKey(dockerContainerString,
                                            &t[i], "Id")) {
                  if (id != NULL) {
                     g_warning("%s:%d: found duplicate key for \"Id\". Json"
                               "has improper format\n", __FUNCTION__, __LINE__);
                     break;
                  }

                  id = g_strdup_printf("%.*s",
                                       t[i + 1].end - t[i + 1].start,
                                       dockerContainerString + t[i + 1].start);
               } else if (ContainerInfoJsonEqIsKey(dockerContainerString,
                                                   &t[i], "Image")) {
                  if (image != NULL) {
                     g_warning("%s:%d: found duplicate key for \"Image\". Json"
                               "has improper format\n", __FUNCTION__, __LINE__);
                     break;
                  }

                  image =
                     g_strdup_printf("%.*s",
                                     t[i + 1].end - t[i + 1].start,
                                     dockerContainerString + t[i + 1].start);
               }
            }

            if (image != NULL && id != NULL) {
               g_debug("%s: Found docker container id: %s and image: %s",
                       __FUNCTION__, id, image);
               g_hash_table_insert(containerTable, id, image);
               id = NULL;
               image = NULL;
               break;
            }
            i++;
         }

         /*
          * Check id and image in the case of (image && !id) and (!image && id)
          */
         if (id != NULL) {
            g_free(id);
         }
         if (image != NULL) {
            g_free(image);
         }
      }
      i++;
   }

exit:
   g_free(t);
   g_free(endpt);
   g_free(dockerContainerString);
   return containerTable;
}
