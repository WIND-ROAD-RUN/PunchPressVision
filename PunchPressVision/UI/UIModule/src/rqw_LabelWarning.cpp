#include "rqw_LabelWarning.h"

namespace rw
{
	namespace rqw
	{
		void LabelWarning::build_ui()
		{
			// 初始化样式
			this->setStyleSheet(QString("QLabel { color: %1; }").arg(_warningColor));
			this->warningInfoList = new WarningInfoList(this);
		}

		void LabelWarning::build_connect()
		{
			// 连接定时器信号到槽函数
			connect(_timerToGray, &QTimer::timeout, this, &LabelWarning::onTimeoutToGray);
			connect(_timerToBlack, &QTimer::timeout, this, &LabelWarning::onTimeoutToBlack);
			connect(this, &ClickableLabel::clicked, this, &LabelWarning::labelClicked);
			connect(warningInfoList, &WarningInfoList::clearWarnings, this, &LabelWarning::clearWarningHistory);
		}

		LabelWarning::LabelWarning(QWidget* parent)
			: ClickableLabel(parent),
			_timerToGray(new QTimer(this)),
			_timerToBlack(new QTimer(this)),
			_maxHistorySize(100),
			_warningColor("red"),
			_timeoutColor("gray"),
			_grayDuration(60000) // 默认灰色持续时间为 60 秒
		{
			build_ui();
			build_connect();
		}

		void LabelWarning::addWarning(const WarningInfo& message, int redDuration)
		{
			// 添加到历史队列
			pushHistory(message);

			// 更新当前警告信息
			_currentMessage = message;
			this->setText(_currentMessage.message);

			// 设置文字为警告颜色
			this->setStyleSheet(QString("QLabel { color: %1; }").arg(_warningColor));

			// 启动红色到灰色的定时器
			_timerToGray->start(redDuration);

			// 停止灰色到黑色的定时器（如果正在运行）
			_timerToBlack->stop();

			pushWarningList(message);
		}

		void LabelWarning::addWarning(const WarningInfo& message, bool updateTimestampIfSame, int redDuration, int time)
		{
			if (updateTimestampIfSame && !_history.empty()) {
				// Find the same warningId in the last 10 seconds
				QDateTime now = QDateTime::currentDateTime();
				for (auto it = _history.rbegin(); it != _history.rend(); ++it) {
					if (it->warningId == message.warningId) {
						qint64 msecsDiff = it->timestamp.msecsTo(now);
						if (msecsDiff >= 0 && msecsDiff <= time) {
							//Find the same warningId in the last 10 seconds
							it->timestamp = now;
							//Update the warning message
							_currentMessage = message;
							this->setText(_currentMessage.message);

							//Reset the timer to gray
							_timerToGray->start(redDuration);
							//Stop the timer to black
							_timerToBlack->stop();

							return;
						}
						// If the id is found but timeout,break directly
						break;
					}
				}
			}

			// If the information is different or the timestamp update function is not enabled, add the warning information according to the normal logic
			addWarning(message, redDuration);
		}

		void LabelWarning::setMaxHistorySize(size_t maxSize)
		{
			_maxHistorySize = maxSize;

			while (_history.size() > _maxHistorySize) {
				_history.pop_front();
			}
		}

		std::deque<WarningInfo> LabelWarning::getHistory() const
		{
			return _history;
		}

		void LabelWarning::setWarningColor(const QString& color)
		{
			_warningColor = color;
		}

		void LabelWarning::setTimeoutColor(const QString& color)
		{
			_timeoutColor = color;
		}

		void LabelWarning::setGrayDuration(int duration)
		{
			_grayDuration = duration;
		}

		void LabelWarning::onTimeoutToGray()
		{
			// 停止红色到灰色的定时器
			_timerToGray->stop();

			// 将文字颜色变为灰色
			this->setStyleSheet(QString("QLabel { color: %1; }").arg(_timeoutColor));

			// 启动灰色到黑色的定时器
			_timerToBlack->start(_grayDuration);
		}

		void LabelWarning::onTimeoutToBlack()
		{
			// 停止灰色到黑色的定时器
			_timerToBlack->stop();

			// 将文字变为黑色并显示 "暂无报警"
			this->setText("暂无报警");
			this->setStyleSheet("QLabel { color: black; }");
		}

		void LabelWarning::labelClicked()
		{
			warningInfoList->setWarningHistory(_history);
			warningInfoList->setWindowFlags(Qt::Window | Qt::CustomizeWindowHint);
			warningInfoList->show();
		}

		void LabelWarning::clearWarningHistory()
		{
			_history.clear();
		}

		// warningList 线程安全操作
		void LabelWarning::pushWarningList(const WarningInfo& info) {
			std::lock_guard<std::mutex> lock(_warningListMutex);
			_warningList.push_back(info);
			if (_warningList.size() > _maxHistorySize) {
				_warningList.pop_front();
			}
		}

		WarningInfo LabelWarning::popWarningListThreadSafe() {
			std::lock_guard<std::mutex> lock(_warningListMutex);
			if (!_warningList.empty()) {
				WarningInfo warning = _warningList.front();
				_warningList.pop_front();
				return warning;
			}
			return WarningInfo();
		}

		WarningInfo LabelWarning::topWarningListThreadSafe()
		{
			std::lock_guard<std::mutex> lock(_warningListMutex);
			if (!_warningList.empty()) {
				return _warningList.front();
			}
			return WarningInfo();
		}

		bool LabelWarning::isEmptyWarningListThreadSafe() const {
			std::lock_guard<std::mutex> lock(_warningListMutex);
			return _warningList.empty();
		}

		std::deque<WarningInfo> LabelWarning::getWarningListThreadSafe() const {
			std::lock_guard<std::mutex> lock(_warningListMutex);
			return _warningList;
		}

		void LabelWarning::clearWarningListThreadSafe() {
			std::lock_guard<std::mutex> lock(_warningListMutex);
			_warningList.clear();
		}

		// history 线程安全操作
		void LabelWarning::pushHistory(const WarningInfo& info) {
			std::lock_guard<std::mutex> lock(_historyMutex);
			_history.push_back(info);
			if (_history.size() > _maxHistorySize) {
				_history.pop_front();
			}
		}

		void LabelWarning::updateLastHistoryTimestamp() {
			std::lock_guard<std::mutex> lock(_historyMutex);
			if (!_history.empty()) {
				_history.back().timestamp = QDateTime::currentDateTime();
			}
		}

		std::deque<WarningInfo> LabelWarning::getHistoryThreadSafe() const {
			std::lock_guard<std::mutex> lock(_historyMutex);
			return _history;
		}

		void LabelWarning::clearHistoryThreadSafe() {
			std::lock_guard<std::mutex> lock(_historyMutex);
			_history.clear();
		}
	}
}