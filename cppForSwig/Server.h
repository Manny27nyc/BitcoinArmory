////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//  Copyright (C) 2016-18, goatpig.                                           //
//  Distributed under the MIT license                                         //
//  See LICENSE-MIT or https://opensource.org/licenses/MIT                    //                                      
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#ifndef _SERVER_H_
#define _SERVER_H_

#include <string.h>
#include <string>
#include <memory>
#include <atomic>
#include <vector>

using namespace std;

#include "include/fcgiapp.h"
#include "include/fcgios.h"
#include "FcgiMessage.h"
#include "WebSocketMessage.h"
#include "libwebsockets.h"

#include "ThreadSafeClasses.h"
#include "BDV_Notification.h"
#include "BinaryData.h"
#include "EncryptionUtils.h"
#include "DataObject.h"
#include "BlockDataManagerConfig.h"
#include "SocketService.h"


class Clients;
class BlockDataManagerThread;

///////////////////////////////////////////////////////////////////////////////
class FCGI_Server
{
   /***
   Figure if it should use a socket or a named pipe.
   Force it to listen only to localhost if we use a socket
   (both in *nix and win32 code files)
   ***/

private:
   SOCKET sockfd_ = -1;
   mutex mu_;
   atomic<bool> run_;
   atomic<uint32_t> liveThreads_;

   const string ip_;
   const string port_;

   unique_ptr<Clients> clients_;
   SocketService keepAliveService_;

private:
   function<void(void)> getShutdownCallback(void)
   {
      auto shutdownCallback = [this](void)->void
      {
         this->haltFcgiLoop();
      };

      return shutdownCallback;
   }

   void passToKeepAliveService(shared_ptr<FCGX_Request>);

public:
   FCGI_Server(BlockDataManagerThread* bdmT, string port, bool listen_all);

   void init(void);
   void enterLoop(void);
   void processRequest(shared_ptr<FCGX_Request> req);
   void haltFcgiLoop(void);
   void shutdown(void);
   void checkSocket(void) const;
};

///////////////////////////////////////////////////////////////////////////////
struct per_session_data__http {
   lws_fop_fd_t fop_fd;
};

struct per_session_data__bdv {
   static const unsigned rcv_size = 8000;
   uint64_t id_;
};

enum demo_protocols {
   /* always first */
   PROTOCOL_HTTP = 0,

   PROTOCOL_ARMORY_BDM,

   /* always last */
   DEMO_PROTOCOL_COUNT
};

int callback_http(struct lws *wsi, enum lws_callback_reasons reason, void *user,
   void *in, size_t len);

///////////////////////////////////////////////////////////////////////////////
struct BDV_packet
{
   uint64_t ID_;
   BinaryData data_;
   struct lws *wsiPtr_;

   BDV_packet(const uint64_t& id, struct lws *wsi) :
      ID_(id), wsiPtr_(wsi)
   {}
};

///////////////////////////////////////////////////////////////////////////////
struct WriteStack
{
   struct lws *wsiPtr_ = nullptr;
   shared_ptr<Stack<BinaryData>> stack_;

   WriteStack(struct lws *wsi) :
      wsiPtr_(wsi)
   {
      stack_ = make_shared<Stack<BinaryData>>();
   }
};

///////////////////////////////////////////////////////////////////////////////
class WebSocketServer
{
private:
   vector<thread> threads_;
   BlockingStack<unique_ptr<BDV_packet>> packetQueue_;
   TransactionalMap<uint64_t, WriteStack> writeMap_;
   TransactionalMap<uint64_t, shared_ptr<WebSocketMessage>> readMap_;

   static atomic<WebSocketServer*> instance_;
   static mutex mu_;
   static promise<bool> shutdownPromise_;
   static shared_future<bool> shutdownFuture_;
   
   unique_ptr<Clients> clients_;
   atomic<unsigned> run_;
   promise<bool> isReadyProm_;

private:
   void webSocketService(void);
   void commandThread(void);
   void setIsReady(void);
   
   static WebSocketServer* getInstance(void);

public:
   WebSocketServer(void);

   static int callback(
      struct lws *wsi, enum lws_callback_reasons reason, 
      void *user, void *in, size_t len);

   static void start(BlockDataManagerThread* bdmT, bool async);
   static void shutdown(void);
   static void write(const uint64_t&, const uint64_t&, Arguments&);
   static void waitOnShutdown(void);
   
   shared_ptr<map<uint64_t, WriteStack>> getWriteMap(void);
   void addId(const uint64_t&, struct lws* ptr);
   void eraseId(const uint64_t&);
};

#endif