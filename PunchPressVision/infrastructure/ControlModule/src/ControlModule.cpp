#include "infrastructure/ControlModule/ControlModule.hpp"

#include <rwul/hoepModbus/hoepModbus_ModbusDevice.hpp>
#include <rwul/hoepModbus/hoepModbus_core.hpp>

namespace inf
{
	namespace
	{
		// PLC 寄存器多采用大端字序；如现场不同在此调整。
		constexpr rw::hoep::Endianness kByteOrder = rw::hoep::Endianness::BigEndian;
	}

	ControlModule::ControlModule() = default;
	ControlModule::~ControlModule() = default;

	void ControlModule::build()
	{
		// TODO(硬件): PLC 的 IP/端口应来自配置（baseCfg.plcIp/plcPort）。
		// build 阶段不主动连接，由上层在进入工作模式时调用 connectToPLC。
	}

	void ControlModule::destroy()
	{
		disconnectPLC();
		modbusDevice_.reset();
	}

	bool ControlModule::connectToPLC(const std::string& ip, int port)
	{
		try
		{
			rw::hoep::ModbusDeviceTcpCfg cfg;
			cfg.ip = ip;
			cfg.port = port;
			cfg.baseAddress = 0;
			modbusDevice_ = std::make_unique<rw::hoep::ModbusDevice>(cfg);

			const bool ok = modbusDevice_->connect();
			connected_.store(ok, std::memory_order_release);
			emit connectionStateChanged(ok);
			return ok;
		}
		catch (...)
		{
			connected_.store(false, std::memory_order_release);
			emit connectionStateChanged(false);
			return false;
		}
	}

	bool ControlModule::disconnectPLC()
	{
		if (!modbusDevice_)
			return true;
		bool ok = true;
		try
		{
			ok = modbusDevice_->disconnect();
		}
		catch (...)
		{
			ok = false;
		}
		connected_.store(false, std::memory_order_release);
		emit connectionStateChanged(false);
		return ok;
	}

	bool ControlModule::isConnected() const
	{
		return connected_.load(std::memory_order_acquire);
	}

	bool ControlModule::writeRegister(int address, uint16_t value)
	{
		if (!modbusDevice_)
			return false;
		try
		{
			return modbusDevice_->writeUInt16Register(
				static_cast<rw::hoep::Address16>(address), value);
		}
		catch (...)
		{
			return false;
		}
	}

	bool ControlModule::readRegister(int address, uint16_t& value)
	{
		if (!modbusDevice_)
			return false;
		try
		{
			return modbusDevice_->readUInt16Register(
				static_cast<rw::hoep::Address16>(address), value);
		}
		catch (...)
		{
			return false;
		}
	}

	bool ControlModule::writeFloat(int address, float value)
	{
		if (!modbusDevice_)
			return false;
		try
		{
			return modbusDevice_->writeFloatRegister(
				static_cast<rw::hoep::Address16>(address), value, kByteOrder);
		}
		catch (...)
		{
			return false;
		}
	}

	bool ControlModule::readFloat(int address, float& value)
	{
		if (!modbusDevice_)
			return false;
		try
		{
			return modbusDevice_->readFloatRegister(
				static_cast<rw::hoep::Address16>(address), value, kByteOrder);
		}
		catch (...)
		{
			return false;
		}
	}

	bool ControlModule::writeMultipleRegisters(int startAddr, const std::vector<uint16_t>& values)
	{
		if (!modbusDevice_)
			return false;
		try
		{
			return modbusDevice_->writeUInt16Registers(
				static_cast<rw::hoep::Address16>(startAddr), values);
		}
		catch (...)
		{
			return false;
		}
	}

	bool ControlModule::readMultipleRegisters(int startAddr, int count, std::vector<uint16_t>& values)
	{
		if (!modbusDevice_)
			return false;
		try
		{
			return modbusDevice_->readUInt16Registers(
				static_cast<rw::hoep::Address16>(startAddr),
				static_cast<rw::hoep::Quantity>(count), values);
		}
		catch (...)
		{
			return false;
		}
	}

	bool ControlModule::writeCoil(int address, bool state)
	{
		if (!modbusDevice_)
			return false;
		try
		{
			return modbusDevice_->writeCoil(
				static_cast<rw::hoep::Address16>(address), state);
		}
		catch (...)
		{
			return false;
		}
	}

	bool ControlModule::readCoil(int address, bool& state)
	{
		if (!modbusDevice_)
			return false;
		try
		{
			return modbusDevice_->readCoil(
				static_cast<rw::hoep::Address16>(address), state);
		}
		catch (...)
		{
			return false;
		}
	}
}
