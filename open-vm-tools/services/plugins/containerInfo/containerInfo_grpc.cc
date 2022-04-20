/*********************************************************
 * Copyright (C) 2021-2022 VMware, Inc. All rights reserved.
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
 * containerInfo_grpc.cc --
 *
 *    This file defines specific functions which are needed to query
 *    the containerd daemon and retrieve the list of running
 *    containers. A gRPC connection is created using the containerd unix
 *    socket and the specified namespace is queried for any running containers.
 */

#include "containerInfoInt.h"
#include "containers.grpc.pb.h"
#include "tasks.grpc.pb.h"
#include <grpc++/grpc++.h>
#include <stdio.h>

using namespace containerd::services::containers::v1;
using namespace containerd::services::tasks::v1;
using namespace containerd::v1::types;
using namespace google::protobuf;


/*
 ******************************************************************************
 * ContainerInfo_GetContainerList --
 *
 * @brief   A gRPC connection is created with the containerd unix
 *          socket and specified namespace is inspected for
 *          running containers.
 *
 * @param[in] ns                       Namespace to be queried.
 * @param[in] containerdSocketPath     Path of the socket.
 * @param[in] maxContainers            Maximum number of containers to be
 *                                     returned in the list.
 *
 * @retval the list of running containers.
 *         NULL if any error occurs or no containers are running.
 *
 ******************************************************************************
 */

GSList *
ContainerInfo_GetContainerList(const char *ns,                   // IN
                               const char *containerdSocketPath, // IN
                               unsigned int maxContainers)       // IN
{
   GSList *containerList = NULL;
   std::shared_ptr<grpc::ChannelInterface> channel;
   std::unique_ptr<Containers::Stub> containerStub;
   std::unique_ptr<Tasks::Stub> taskStub;
   grpc::Status status;
   int i;
   unsigned int containersAdded;
   const ListContainersRequest req;
   std::unique_ptr<ListContainersResponse> res;
   grpc::ClientContext containerContext;
   static const std::string namespaceKey = "containerd-namespace";
   gchar *unixSocket = NULL;

   if (ns == NULL || containerdSocketPath == NULL) {
      g_warning("%s: Invalid arguments specified.\n", __FUNCTION__);
      goto exit;
   }

   unixSocket = g_strdup_printf("unix://%s", containerdSocketPath);

   containerContext.AddMetadata(namespaceKey, ns);

   channel =
      grpc::CreateChannel(unixSocket, grpc::InsecureChannelCredentials());

   if (channel == nullptr) {
      g_warning("%s: Failed to create gRPC channel\n", __FUNCTION__);
      goto exit;
   }

   containerStub = Containers::NewStub(channel);
   if (containerStub == nullptr) {
      g_warning("%s: Failed to create containerStub\n", __FUNCTION__);
      goto exit;
   }

   taskStub = Tasks::NewStub(channel);
   if (taskStub == nullptr) {
      g_warning("%s: Failed to create taskStub\n", __FUNCTION__);
      goto exit;
   }

   res = std::make_unique<ListContainersResponse>();
   status = containerStub->List(&containerContext, req, res.get());

   if (!status.ok()) {
      g_warning("%s: Failed to list containers. Error: %s\n", __FUNCTION__,
                status.error_message().c_str());
      goto exit;
   }

   g_debug("%s: Namespace: '%s', number of containers found: %d", __FUNCTION__,
           ns, res->containers_size());

   for (i = 0, containersAdded = 0;
        i < res->containers_size() && containersAdded < maxContainers; i++) {
      Container curContainer = res->containers(i);
      std::string id = curContainer.id();
      std::string image = curContainer.image();
      ContainerInfo *info;
      GetRequest taskReq;
      std::unique_ptr<GetResponse> taskRes;
      grpc::Status taskStatus;
      grpc::ClientContext taskContext;

      taskContext.AddMetadata(namespaceKey, ns);

      /*
       * Get pid for container using taskStub.
       */
      taskRes = std::make_unique<GetResponse>();
      taskReq.set_container_id(id);
      taskStatus = taskStub->Get(&taskContext, taskReq, taskRes.get());
      if (!taskStatus.ok()) {
         g_debug("%s: Task get service failed: %s. skipping container: %s\n",
                 __FUNCTION__, taskStatus.error_message().c_str(), id.c_str());
         continue;
      }

      info = (ContainerInfo *)g_malloc(sizeof(*info));
      info->id = g_strdup(id.c_str());
      info->image = g_strdup(image.c_str());

      g_debug("%s: Found container id: %s and image: %s\n", __FUNCTION__,
              info->id, info->image);
      containerList = g_slist_prepend(containerList, info);
      containersAdded++;
   }

exit:
   g_free(unixSocket);
   return containerList;
}
