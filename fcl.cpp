#include "stdafx.h"

namespace fcl {
	template <>
	void writer::write_item(unsigned id, const char* const & item)
	{
		m_output->write_lendian_t(id, m_abort);
		m_output->write_string(item, m_abort);
	};

	template <>
	void writer::write_item(unsigned id, const cfg_string& item)
	{
		write_item(id, item.get_ptr());
	};

	template <>
	void writer::write_item(unsigned id, const pfc::string8& item)
	{
		write_item(id, item.get_ptr());
	};

	template <>
	void writer::write_item(unsigned id, const LOGFONT& lfc)
	{
		LOGFONT lf = lfc;
		t_size face_len = pfc::wcslen_max(lf.lfFaceName, tabsize(lf.lfFaceName));

		if (face_len < tabsize(lf.lfFaceName)) {
			WCHAR* ptr = lf.lfFaceName;
			ptr += face_len;
			memset(ptr, 0, sizeof(WCHAR) * (tabsize(lf.lfFaceName) - face_len));
		}

		m_output->write_lendian_t(id, m_abort);
		m_output->write_lendian_t(sizeof(lf), m_abort);

		m_output->write_lendian_t(lf.lfHeight, m_abort);
		m_output->write_lendian_t(lf.lfWidth, m_abort);
		m_output->write_lendian_t(lf.lfEscapement, m_abort);
		m_output->write_lendian_t(lf.lfOrientation, m_abort);
		m_output->write_lendian_t(lf.lfWeight, m_abort);

		//meh endianness
		m_output->write(&lf.lfItalic, 8 + sizeof(lf.lfFaceName), m_abort);
	}

	template <>
	void writer::write_item(unsigned id, const cfg_struct_t<LOGFONT>& cfg_lfc)
	{
		write_item(id, cfg_lfc.get_value());
	}

	template <>
	void reader::read_item(LOGFONT& lf_out)
	{
		LOGFONT lf;

		read_item(lf.lfHeight);
		read_item(lf.lfWidth);
		read_item(lf.lfEscapement);
		read_item(lf.lfOrientation);
		read_item(lf.lfWeight);

		//meh endianness
		read(&lf.lfItalic, 8 + sizeof(lf.lfFaceName));

		lf_out = lf;
	}

	template <>
	void reader::read_item(cfg_struct_t<LOGFONT>& cfg_out)
	{
		read_item(cfg_out.get_value());
	}
}
