#pragma once

enum epoll_tag
{
	EPOLL_TAG_INVALID              = 0,
	EPOLL_TAG_TENANT_SERVER_SOCKET = 1,
	EPOLL_TAG_METRICS_SERVER_SOCKET,
	EPOLL_TAG_HTTP_SESSION_CLIENT_SOCKET,
};
