#pragma once

template <typename t_type>
class config_item_t {
	cfg_int_t<t_type> m_value;
public:

	void reset() { set(get_default_value()); }
	void set(t_type p_val) { m_value = p_val; on_change(); }
	t_type get() const { return m_value; };

	virtual t_type get_default_value() = 0;
	virtual void on_change() = 0;
	virtual const GUID & get_guid() = 0;
	config_item_t(const GUID & p_guid, t_type p_value) : m_value(p_guid, p_value)
	{};
};

template<>
class config_item_t<pfc::string8> {
	cfg_string m_value;
public:
	void reset() { set(get_default_value()); }
	void set(const char * p_val) { m_value = p_val; on_change(); }
	const char * get() const { return m_value; };

	virtual const char * get_default_value() = 0;
	virtual void on_change() = 0;
	virtual const GUID & get_guid() = 0;
	config_item_t(const GUID & p_guid, const char * p_value) : m_value(p_guid, p_value)
	{};
};

namespace uih {
	template <typename TInteger>
	class IntegerAndDpi {
	public:
		TInteger value;
		uint32_t dpi;

		IntegerAndDpi(TInteger _value = NULL) : value(_value), dpi(USER_DEFAULT_SCREEN_DPI) {};
	};

	template<typename TInteger>
	class ConfigIntegerDpiAware {
	public:
		using ValueType = IntegerAndDpi<TInteger>;
		using Type = ConfigIntegerDpiAware<TInteger>;

		// X and Y DPIs are always the same for 'Windows apps', according to MSDN.
		// https://msdn.microsoft.com/en-us/library/windows/desktop/dn312083%28v=vs.85%29.aspx
		// https://msdn.microsoft.com/en-us/library/windows/desktop/dn280510%28v=vs.85%29.aspx
		void set(TInteger value, uint32_t dpi = GetSystemDpiCached().cx)
		{
			m_Value.get_value().value = value;
			m_Value.get_value().dpi = dpi;
			on_change();
		}
		Type & operator = (TInteger value) { set(value);  return *this; }

		operator TInteger () const { return getScaledValue(); }
		const ValueType & getRawValue() const { return m_Value; };

		TInteger getScaledValue() const { return MulDiv(m_Value.get_value().value, GetSystemDpiCached().cx, m_Value.get_value().dpi); };

		virtual void on_change() {};
		ConfigIntegerDpiAware(const GUID & guid, TInteger value) : m_Value(guid, ValueType(value))
		{};
	private:
		cfg_struct_t < IntegerAndDpi<TInteger> > m_Value;
	};

	using ConfigUint32DpiAware = ConfigIntegerDpiAware<uint32_t>;
	using ConfigInt32DpiAware = ConfigIntegerDpiAware<int32_t>;
}