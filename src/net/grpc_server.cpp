#include "grpc_server.hpp"
#include <grpc++/impl/codegen/service_type.h>
#include "util/common.hpp"

namespace deepfabric
{
GrpcServer::GrpcServer(const std::string& listeningPointAddress)
    :running_(false)
{
	builder_.AddListeningPort(listeningPointAddress, grpc::InsecureServerCredentials());
}

GrpcServer::~GrpcServer()
{
}

void GrpcServer::AddService(std::unique_ptr<grpc::Service> service)
{
	ASSERT(running_ == false);
	builder_.RegisterService(service.get());
	services_.emplace_back(service.release());
}

void GrpcServer::Start()
{
	ASSERT(running_.exchange(true) == false);
	server_ = builder_.BuildAndStart();
}

}