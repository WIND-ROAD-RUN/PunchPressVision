#pragma once

#include <memory>


namespace rw::oso
{
	class StorageContext;
}

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
