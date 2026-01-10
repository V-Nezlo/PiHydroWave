#ifndef BLACKBOARDENTRY_HPP
#define BLACKBOARDENTRY_HPP

#include <core/Blackboard.hpp>
#include <memory>
#include <type_traits>

template<typename T>
class BlackboardEntry {
public:
	BlackboardEntry(std::string aName, std::shared_ptr<Blackboard> aBb) : name{aName}, bb{aBb}
	{
	}

	bool set(T aValue)
	{
		if constexpr (std::is_enum_v<T>) {
			int value = static_cast<int>(aValue);
			return bb->set(name, value);
		} else {
			return bb->set(name, aValue);
		}
	}

	bool present() const
	{
		return bb->has(name);
	}

	T read() const
	{
		if constexpr (std::is_enum_v<T>) {
			int value = bb->get<int>(name).value();
			return static_cast<T>(value);
		} else {
			return bb->get<T>(name).value();
		}
	}

	void subscribe(AbstractEntryObserver *aObs)
	{
		return bb->subscribe(name, aObs);
	}

	auto operator=(T aValue)
	{
		return set(aValue);
	}

	T operator()() const
	{
		return read();
	}

	std::string_view getName() const
	{
		return name;
	}

private:
	std::string name;
	std::shared_ptr<Blackboard> bb;
};

#endif // BLACKBOARDENTRY_HPP
