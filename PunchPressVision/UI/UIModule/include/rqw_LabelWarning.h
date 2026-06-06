#pragma once

#include <QTimer>
#include <deque>
#include "rqw_LabelClickable.h"
#include "WarningInfoList.h"
#include"WarnUtilty.hpp"
#include <mutex>

namespace rw
{
	namespace rqw
	{
		class LabelWarning : public ClickableLabel
		{
			Q_OBJECT
		private:
			WarningInfoList* warningInfoList;

		private:
			void build_ui();
			void build_connect();

		public:
			explicit LabelWarning(QWidget* parent = nullptr);

			// 设置队列最大容量
			void setMaxHistorySize(size_t maxSize);

			// 获取历史警告信息
			std::deque<WarningInfo> getHistory() const;

			// 设置警告颜色
			void setWarningColor(const QString& color);

			// 设置超时后的颜色
			void setTimeoutColor(const QString& color);

			// 设置灰色状态持续时间
			void setGrayDuration(int duration);
		public:
			void addWarning(const WarningInfo& message, int redDuration);
			void addWarning(const WarningInfo& message, bool updateTimestampIfSame = true, int redDuration = 5000, int time = 10000);
		public:
			// warningList
			void pushWarningList(const WarningInfo& info);
			WarningInfo popWarningListThreadSafe();
			WarningInfo topWarningListThreadSafe();
			bool isEmptyWarningListThreadSafe() const;
			std::deque<WarningInfo> getWarningListThreadSafe() const;
			void clearWarningListThreadSafe();

			// history
			void pushHistory(const WarningInfo& info);
			void updateLastHistoryTimestamp();
			std::deque<WarningInfo> getHistoryThreadSafe() const;
			void clearHistoryThreadSafe();
		private slots:
			// 槽函数：将文字颜色变为灰色
			void onTimeoutToGray();

			// 槽函数：将文字变为黑色 "暂无报警"
			void onTimeoutToBlack();

			void labelClicked();

		private slots:
			void clearWarningHistory(); // 清空历史记录

		private:
			QTimer* _timerToGray;    // 定时器，用于控制红色变灰色
			QTimer* _timerToBlack;   // 定时器，用于控制灰色变黑色
			WarningInfo _currentMessage; // 当前警告信息
			size_t _maxHistorySize;  // 队列最大容量

			QString _warningColor;   // 警告颜色
			QString _timeoutColor;   // 超时后的颜色
			int _grayDuration;       // 灰色状态持续时间（毫秒）
		public:
			std::deque<WarningInfo> _warningList; // 历史警告信息队列,供外部获取使用
			std::deque<WarningInfo> _history; // 历史警告信息队列
			mutable std::mutex _historyMutex;      // 新增
			mutable std::mutex _warningListMutex;  // 新增
		};
	}
}