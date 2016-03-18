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

		IntegerAndDpi(TInteger _value = NULL, uint32_t _dpi = USER_DEFAULT_SCREEN_DPI) : value(_value), dpi(_dpi) {};
	};

	template<typename TInteger>
	class ConfigIntegerDpiAware : public cfg_var {
	public:
		using ValueType = IntegerAndDpi<TInteger>;
		using Type = ConfigIntegerDpiAware<TInteger>;

		// X and Y DPIs are always the same for 'Windows apps', according to MSDN.
		// https://msdn.microsoft.com/en-us/library/windows/desktop/dn312083%28v=vs.85%29.aspx
		// https://msdn.microsoft.com/en-us/library/windows/desktop/dn280510%28v=vs.85%29.aspx
		void set(TInteger value, uint32_t dpi = GetSystemDpiCached().cx)
		{
			m_Value.value = value;
			m_Value.dpi = dpi;
			on_change();
		}
		void set(IntegerAndDpi<TInteger> value)
		{
			m_Value = value;
			on_change();
		}
		Type & operator = (TInteger value) { set(value);  return *this; }
		Type & operator = (IntegerAndDpi<TInteger> value) { set(value);  return *this; }

		operator TInteger () const { return getScaledValue(); }
		const ValueType & getRawValue() const { return m_Value; };

		TInteger getScaledValue() const { return MulDiv(m_Value.value, GetSystemDpiCached().cx, m_Value.dpi); };

		virtual void on_change() {};
		ConfigIntegerDpiAware(const GUID & guid, TInteger value) : cfg_var(guid), m_Value(ValueType(value))
		{};
	protected:
		void get_data_raw(stream_writer * p_stream, abort_callback & p_abort)
		{
			p_stream->write_lendian_t(m_Value.value, p_abort);
			p_stream->write_lendian_t(m_Value.dpi, p_abort);
		}
		void set_data_raw(stream_reader * p_stream, t_size p_sizehint, abort_callback & p_abort)
		{
			p_stream->read_lendian_t(m_Value.value, p_abort);
			// Allow migration from older config variables
			if (p_sizehint > sizeof(TInteger))
				p_stream->read_lendian_t(m_Value.dpi, p_abort);
			else
				m_Value.dpi = GetSystemDpiCached().cx; //If migrating from an older config var, assume it was set using the current system DPI.
		}
	private:
		IntegerAndDpi<TInteger> m_Value;
	};

	using ConfigUint32DpiAware = ConfigIntegerDpiAware<uint32_t>;
	using ConfigInt32DpiAware = ConfigIntegerDpiAware<int32_t>;
}