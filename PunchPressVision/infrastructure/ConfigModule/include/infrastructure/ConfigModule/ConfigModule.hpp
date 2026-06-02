#pragma once

#include "rwul/oso/oso_StorageContext.hpp"

namespace inf
{
	class ConfigModule
	{
	public:
		ConfigModule();
		~ConfigModule();
	private:
		std::unique_ptr<rw::oso::StorageContext> storageContext_;
	public:
		void build();
		void destroy();
	};
}
