#pragma once

#include <QObject>

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

	/// <summary>
	/// infTool 层统一抽象接口。
	/// 组合 QObject（信号槽）与 IBusiness（生命周期），为所有视觉算法工具类提供公共基类。
	/// 子类通过 Q_OBJECT 声明各自的信号槽，并实现 IBusiness 生命周期方法。
	/// </summary>
	class IInfTool
		: public QObject
		, public IBusiness
	{
		Q_OBJECT

	public:
		~IInfTool() override = default;
	};
}
