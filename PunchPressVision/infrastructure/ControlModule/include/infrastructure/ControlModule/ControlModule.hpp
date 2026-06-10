#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <QObject>

#include "global/GlobalInterface.hpp"

namespace rw::hoep
{
	class ModbusDevice;
}

namespace inf
{
	// PLC 控制网关：封装 Modbus TCP 通信（rw::hoep::ModbusDevice）。
	// 寄存器地址映射（LLD 附录）：
	//   40001 float  X 偏移量
	//   40003 float  Y 偏移量
	//   40005 float  旋转角度 Angle
	//   40007 int    定位结果有效标志
	//   40101 int    九点标定到位确认（PLC→系统）
	//   40102 int    九点标定当前点位序号
	//   40103~ float  九点点位机械坐标
	class ControlModule
		: public QObject, public global::IInfrastructure
	{
		Q_OBJECT
	public:
		ControlModule();
		~ControlModule() override;

		void build() override;
		void destroy() override;

		// 连接管理。ip/port 默认取自配置，未配置时连接失败仅告警。
		bool connectToPLC(const std::string& ip, int port);
		bool disconnectPLC();
		bool isConnected() const;

		// 单寄存器读写（保持寄存器，16 位）
		bool writeRegister(int address, uint16_t value);
		bool readRegister(int address, uint16_t& value);

		// 浮点读写（占用 2 个寄存器）
		bool writeFloat(int address, float value);
		bool readFloat(int address, float& value);

		// 批量寄存器读写（九点标定优化）
		bool writeMultipleRegisters(int startAddr, const std::vector<uint16_t>& values);
		bool readMultipleRegisters(int startAddr, int count, std::vector<uint16_t>& values);

		// 线圈读写
		bool writeCoil(int address, bool state);
		bool readCoil(int address, bool& state);

	signals:
		void connectionStateChanged(bool connected);

	private:
		std::unique_ptr<rw::hoep::ModbusDevice> modbusDevice_;
		std::atomic_bool connected_{ false };
	};
}
