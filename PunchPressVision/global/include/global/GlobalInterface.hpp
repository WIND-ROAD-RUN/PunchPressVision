#pragma once

namespace global
{
	class IInfrastructure
	{
	public:
		virtual ~IInfrastructure() = default;
		virtual void build() = 0;
		virtual void destroy() = 0;
	};

	class IBusiness
	{
	public:
		virtual ~IBusiness() = default;
		virtual void build()=0;
		virtual void destroy()=0;
		virtual void start()=0;
		virtual void stop()=0;
	};
}
