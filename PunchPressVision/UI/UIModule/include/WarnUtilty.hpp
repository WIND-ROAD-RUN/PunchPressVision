#pragma once

#include<QString>
#include<QDateTime>

namespace rw
{
	namespace rqw
	{
		enum class WarningType
		{
			Warning,
			Error,
			Info
		};

		struct WarningInfo
		{
		public:
			WarningType type{ WarningType::Info };
			QString message{ "" };
			QDateTime timestamp{ QDateTime::currentDateTime() };
		public:
			int warningId{ -1 };
		};
	}
}