#ifndef __PHYS_VEHICLE_HPP__
#define __PHYS_VEHICLE_HPP__

#include "pragma/networkdefinitions.h"
#include "pragma/physics/base.hpp"
#include <optional>

namespace pragma::physics
{
	class IConvexHullShape;
	constexpr auto DEFAULT_CHASSIS_MASS = 1'500.f;
	constexpr auto DEFAULT_WHEEL_MASS = 20.f;
	constexpr auto DEFAULT_WHEEL_WIDTH = 16.f;
	constexpr auto DEFAULT_WHEEL_RADIUS = 20.f;
	struct DLLNETWORK ChassisCreateInfo
	{
		float mass = DEFAULT_CHASSIS_MASS;
		Vector3 momentOfInertia = {};
		Vector3 cmOffset = {};
		int32_t shapeIndex = -1;
	};

	struct DLLNETWORK WheelCreateInfo
	{
		struct DLLNETWORK SuspensionInfo
		{
			float maxCompression = 0.3f;
			float maxDroop = 0.1f;
			float springStrength = 35'000.0f;
			float springDamperRate = 4'500.0f;

			float camberAngleAtRest = 0.f;
			float camberAngleAtMaxDroop = 0.01f;
			float camberAngleAtMaxCompression = -0.01f;
		};

		static WheelCreateInfo CreateStandardFrontWheel();
		static WheelCreateInfo CreateStandardRearWheel();
		enum class Flags : uint32_t
		{
			None = 0u,
			Front = 1u,
			Rear = Front<<1u,
			Left = Rear<<1u,
			Right = Left<<1u
		};
		Flags flags = Flags::None;
		float mass = DEFAULT_WHEEL_MASS;
		float width = DEFAULT_WHEEL_WIDTH;
		float radius = DEFAULT_WHEEL_RADIUS;
		float maxHandbrakeTorque = 0.f;
		// The index of the shape of the vehicle's rigid body
		// for this wheel. -1 means no shape is associated with
		// the wheel.
		int32_t shapeIndex = -1;
		umath::Degree maxSteeringAngle = 0.f;
		Vector3 chassisOffset = {};
		SuspensionInfo suspension = {};

		std::optional<float> momentOfInertia = {};
		// Returns moi if specified, otherwise calculates
		// moi for a cylinder of the specified radius and mass.
		float GetMomentOfInertia() const;
	};

	class IRigidBody;
	struct DLLNETWORK VehicleCreateInfo
	{
		static constexpr uint32_t WHEEL_COUNT_4W_DRIVE = 4u;
		enum class WheelDrive : uint8_t
		{
			Front = 0u,
			Rear,
			Four
		};
		enum class Wheel : uint8_t
		{
			// Note: Order is important!
			FrontLeft = 0,
			FrontRight,
			RearLeft,
			RearRight,

			Dummy
		};
		struct DLLNETWORK AntiRollBar
		{
			AntiRollBar(Wheel wheel0,Wheel wheel1,float stiffness=10'000.0f)
				: wheel0{wheel0},wheel1{wheel1},stiffness{stiffness}
			{}
			AntiRollBar()=default;
			Wheel wheel0 = Wheel::FrontLeft;
			Wheel wheel1 = Wheel::FrontRight;
			float stiffness = 10'000.0f;
		};
		static Wheel GetWheelType(const WheelCreateInfo &wheelDesc);
		static VehicleCreateInfo CreateStandardFourWheelDrive(
			const std::array<Vector3,WHEEL_COUNT_4W_DRIVE> &wheelCenterOffsets,
			float chassisMass=DEFAULT_CHASSIS_MASS,
			float wheelMass=DEFAULT_WHEEL_MASS,
			float wheelWidth=DEFAULT_WHEEL_WIDTH,
			float wheelRadius=DEFAULT_WHEEL_RADIUS
		);

		ChassisCreateInfo chassis = {};
		std::vector<WheelCreateInfo> wheels = {};
		WheelDrive wheelDrive = WheelDrive::Four;
		std::vector<AntiRollBar> antiRollBars = {};
		float maxEngineTorque = 500.f;
		umath::Radian maxEngineRotationSpeed = 600.f;
		float gearSwitchTime = 0.5f;
		float clutchStrength = 10.f;
		float gravityFactor = 1.f; // TODO
		util::TSharedHandle<IRigidBody> actor = nullptr;
	};

	class ICollisionObject;
	class DLLNETWORK IVehicle
		: public IBase,public IWorldObject
	{
	public:
		enum class Gear : uint16_t
		{
			Reverse = 0,
			Neutral,
			First,
			Second,
			Third,
			Fourth,
			Fifth,
			Sixth,
			Seventh,
			Eighth,
			Ninth,
			Tenth,
			Eleventh,
			Twelfth,
			Thirteenth,
			Fourteenth,
			Fifteenth,
			Sixteenth,
			Seventeenth,
			Eighteenth,
			Nineteenth,
			Twentieth,
			Twentyfirst,
			Twentysecond,
			Twentythird,
			Twentyfourth,
			Twentyfifth,
			Twentysixth,
			Twentyseventh,
			Twentyeighth,
			Twentyninth,
			Thirtieth,

			Count
		};

		using WheelIndex = uint32_t;

		virtual void OnRemove() override;

		ICollisionObject *GetCollisionObject();
		const ICollisionObject *GetCollisionObject() const;
		virtual void InitializeLuaObject(lua_State *lua) override;

		virtual void SetUseDigitalInputs(bool bUseDigitalInputs)=0;

		virtual void SetBrakeFactor(float f)=0;
		virtual void SetHandbrakeFactor(float f)=0;
		virtual void SetAccelerationFactor(float f)=0;
		virtual void SetTurnFactor(float f)=0;

		virtual void SetGear(Gear gear)=0;
		virtual void SetGearDown()=0;
		virtual void SetGearUp()=0;
		virtual void SetGearSwitchTime(float time)=0;
		virtual void ChangeToGear(Gear gear)=0;
		virtual void SetUseAutoGears(bool useAutoGears)=0;

		virtual bool ShouldUseAutoGears() const=0;
		virtual Gear GetCurrentGear() const=0;
		virtual float GetEngineRotationSpeed() const=0;

		virtual void SetRestState()=0;

		virtual void ResetControls()=0;

		virtual void SetWheelRotationAngle(WheelIndex wheel,umath::Radian angle)=0;
		virtual void SetWheelRotationSpeed(WheelIndex wheel,umath::Radian speed)=0;

		virtual bool IsInAir() const=0;

		virtual uint32_t GetWheelCount() const=0;
		virtual float GetForwardSpeed() const=0;
		virtual float GetSidewaysSpeed() const=0;
	protected:
		IVehicle(IEnvironment &env,const util::TSharedHandle<ICollisionObject> &collisionObject);
		virtual bool ShouldUseDigitalInputs() const=0;
		util::TSharedHandle<ICollisionObject> m_collisionObject = nullptr;
	};
	
	class DLLNETWORK IWheel
		: public IBase
	{
	public:
	protected:
		IWheel(IEnvironment &env);
	};
};
REGISTER_BASIC_BITWISE_OPERATORS(pragma::physics::WheelCreateInfo::Flags)

#endif
