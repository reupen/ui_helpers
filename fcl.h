#pragma once

namespace fcl {
	class writer {
	public:
		template <typename t_item>
		void write_raw(const t_item& item)
		{
			pfc::assert_raw_type<t_item>();
			m_output->write_lendian_t(item, m_abort);
		};

		void write_item(unsigned id, const void* item, t_size size)
		{
			m_output->write_lendian_t(id, m_abort);
			m_output->write_lendian_t(size, m_abort);
			m_output->write(item, size, m_abort);
		};

		template <typename t_item>
		void write_item(unsigned id, const t_item& item)
		{
			pfc::assert_raw_type<t_item>();
			m_output->write_lendian_t(id, m_abort);
			m_output->write_lendian_t(sizeof(item), m_abort);
			m_output->write_lendian_t(item, m_abort);
		};

		template <typename t_int_type>
		void write_item(unsigned id, const cfg_int_t<t_int_type>& item)
		{
			m_output->write_lendian_t(id, m_abort);
			m_output->write_lendian_t(sizeof(t_int_type), m_abort);
			m_output->write_lendian_t(t_int_type(item.get_value()), m_abort);
		};

		template <typename t_int_type>
		void write_item_config(unsigned id, const config_item_t<t_int_type>& item)
		{
			m_output->write_lendian_t(id, m_abort);
			m_output->write_lendian_t(sizeof(t_int_type), m_abort);
			m_output->write_lendian_t(t_int_type(item.get()), m_abort);
		};

		writer(stream_writer* p_out, abort_callback& p_abort)
			: m_output(p_out), m_abort(p_abort) {} ;

	private:
		stream_writer* m_output;
		abort_callback& m_abort;
	};

	template <>
	void writer::write_item(unsigned id, const char* const & item);

	template <>
	void writer::write_item(unsigned id, const cfg_string& item);

	template <>
	void writer::write_item(unsigned id, const pfc::string8& item);

	template <>
	void writer::write_item(unsigned id, const LOGFONT& lfc);

	template <>
	void writer::write_item(unsigned id, const cfg_struct_t<LOGFONT>& cfg_lfc);

	class reader {
	public:
		template <typename t_item>
		void read_item(t_item& p_out)
		{
			pfc::assert_raw_type<t_item>();
			m_input->read_lendian_t(p_out, m_abort);
			m_position += sizeof (t_item);
		};

		void read(void* pout, t_size size)
		{
			t_size pread = m_input->read(pout, size, m_abort);
			m_position += pread;
			if (pread != size)
				throw exception_io_data_truncation();
		};

		template <typename t_int_type>
		void read_item(cfg_int_t<t_int_type>& p_out)
		{
			pfc::assert_raw_type<t_int_type>();
			t_int_type temp;
			m_input->read_lendian_t(temp, m_abort);
			p_out = temp;
			m_position += sizeof (t_int_type);
		};

		void read_item(cfg_string& p_out, t_size size)
		{
			read_item((pfc::string8&)p_out, size);
		};

		void read_item(pfc::string8& p_out, t_size size)
		{
			pfc::array_t<char> temp;
			temp.set_size(size + 1);
			temp.fill(0);

			if (size) {
				unsigned read = 0;
				read = m_input->read(temp.get_ptr(), size, m_abort);
				if (read != size)
					throw exception_io_data_truncation();
			}
			p_out = temp.get_ptr();
			m_position += size;
		};

		template <typename t_int_type>
		void read_item_config(config_item_t<t_int_type>& p_out)
		{
			pfc::assert_raw_type<t_int_type>();
			t_int_type temp;
			m_input->read_lendian_t(temp, m_abort);
			p_out.set(temp);
			m_position += sizeof (t_int_type);
		};

		template <typename t_int_type>
		void read_item_config(uih::ConfigItem<t_int_type>& p_out)
		{
			pfc::assert_raw_type<t_int_type>();
			t_int_type temp;
			m_input->read_lendian_t(temp, m_abort);
			p_out = temp;
			m_position += sizeof(t_int_type);
		};

		/**
		 * \brief Reads an item using a temporary variable.
		 * 
		 * \tparam t_int_type	The type of the object to read
		 * \return				The read value
		 */
		template <typename t_int_type>
		t_int_type read_raw_item()
		{
			pfc::assert_raw_type<t_int_type>();
			t_int_type temp;
			m_input->read_lendian_t(temp, m_abort);
			m_position += sizeof(t_int_type);
			return temp;
		}

		template <typename t_item>
		void read_item_force_bool(t_item& p_out)
		{
			bool temp;
			m_input->read_lendian_t(temp, m_abort);
			p_out = temp;
			m_position += sizeof (bool);
		};

		template <typename t_int_type>
		void read_item_force_bool(cfg_int_t<t_int_type>& p_out)
		{
			bool temp;
			m_input->read_lendian_t(temp, m_abort);
			p_out = temp;
			m_position += sizeof (bool);
		};

		t_size get_remaining()
		{
			return m_position > m_size ? 0 : m_size - m_position;
		}

		void skip(t_size delta)
		{
			auto read = static_cast<size_t>(m_input->skip(delta, m_abort));
			m_position += read;
			if (read != delta)
				throw exception_io_data_truncation();
		}

		reader(stream_reader* p_input, t_size size, abort_callback& p_abort)
			: m_size(size), m_position(0), m_input(p_input), m_abort(p_abort) {}

		reader(reader& p_reader, t_size size, abort_callback& p_abort)
			: m_size(size), m_position(0), m_input(p_reader.m_input), m_abort(p_abort)
		{
			p_reader.m_position += size;
		};

	private:
		t_size m_size, m_position;
		stream_reader* m_input;
		abort_callback& m_abort;
	};

	template <>
	void reader::read_item(LOGFONT& lf_out);

	template <>
	void reader::read_item(cfg_struct_t<LOGFONT>& cfg_out);
}
