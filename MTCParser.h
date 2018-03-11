#ifndef MTCParser_H
#define MTCParser_H

#include "RingQueue.h"

// ---------- Full Frame Message (FFM) ----------
// F0 7F [chan] 01 [sub - ID 2] hr mn sc fr 0xF7
// -> 0xF0 0x7F 0x7F 0x01 0x01 hh mm ss ff 0xF7
//
// F0 7F = Real Time Universal System Exclusive Header
// [chan] = 7F (message intended for entire system)
// 01 = [sub - ID 1], 'MIDI Time Code'
// [sub - ID 2] = 01, Full Time Code Message
// hr = hours and type: 0 yy zzzzz
// yy = type :
//	00 = 24 Frames / Second
//	01 = 25 Frames / Second
//	10 = 30 Frames / Second(drop frame)
//	11 = 30 Frames / Second(non - drop frame)
//	zzzzz = Hours(00->23)
//	mn = Minutes(00->59)
//	sc = Seconds(00->59)
//	fr = Frames(00->29)
//	F7 = EOX

// ---------- Quarter Frame Message ----------
// F1 0nnn dddd
//
// dddd = 4 bits of binary data for this Message Type
// nnn = Message Type :
//   0 = Frame count LS nibble
//   1 = Frame count MS nibble
//   2 = Seconds count LS nibble
//   3 = Seconds count MS nibble
//   4 = Minutes count LS nibble
//   5 = Minutes count MS nibble
//   6 = Hours count LS nibble
//   7 = Hours count MS nibble and SMPTE Type
//
// Piece Data byte Significance
// 0     0000 ffff Frame number lsbits
// 1     0001 000f Frame number msbit
// 2     0010 ssss Second lsbits
// 3     0011 00ss Second msbits
// 4     0100 mmmm Minute lsbits
// 5     0101 00mm Minute msbits
// 6     0110 hhhh Hour lsbits
// 7     0111 0rrh Rate and hour msbit


class MTCParser
{
	struct MTCPacket
	{
		uint8_t type;
		uint8_t hour;
		uint8_t minute;
		uint8_t second;
		uint8_t frame;
	};
	
public:

	inline uint8_t available() const { return mtc_packet_.size(); }
	inline void pop() { mtc_packet_.pop(); }

	inline uint8_t type() const { return mtc_packet_.front().type; }
	inline uint8_t hour() const { return mtc_packet_.front().hour; }
	inline uint8_t minute() const { return mtc_packet_.front().minute; }
	inline uint8_t second() const { return mtc_packet_.front().second; }
	inline uint8_t frame() const { return mtc_packet_.front().frame; }
	
	inline float asSeconds() const
	{
		const auto& t = mtc_packet_.front();
		return t.hour * 60. * 60. + t.minute * 60. + t.second + t.frame * MTCFrameSecond[t.type];
	}
	inline float asMillis() const { return asSeconds() * 0.001; }
	inline float asMicros() const { return asMillis() * 0.001; }
	inline int32_t asFrameCount() const { return asSeconds() * MTCFrameRate[mtc_packet_.front().type]; }
	inline string asString() const
	{
		stringstream ss;

		if (type() == FPS_29_97)
			ss << to_string(hour())   << ":"
			   << to_string(minute()) << ":"
		       << to_string(second()) << ";" 
		       << to_string(frame());
		else
			ss << to_string(hour())   << ":" 
		       << to_string(minute()) << ":" 
		       << to_string(second()) << ":" 
		       << to_string(frame());

		return ss.str();
	}

	inline void feed(const uint8_t* const data, const size_t size)
	{
		for (size_t i = 0; i < size; ++i) feed(data[i]);
	}

	inline void feed(const uint8_t data)
	{
		switch (state)
		{
			// common path for FFM & QFM
			case State::Header:
			{
				if (data == FFM_Header_1)
				{
					state = State::FFM_Header_2;
					clearBuffer();
				}
				else if (data == QFM_Header)
				{
					state = State::QFM_Value;
				}
				else
				{
					state = State::Header;
					clearBuffer();
				}
				break;
			}

			// for Full Frame Message (FFM)
			case State::FFM_Header_2:
			{
				if (data == FFM_Header_2) state = State::FFM_Channel;
				else state = State::Header;
				break;
			}
			case State::FFM_Channel:
			{
				if (data == FFM_Channel) state = State::FFM_ID_1;
				else state = State::Header;
				break;
			}
			case State::FFM_ID_1:
			{
				if (data == FFM_ID_1) state = State::FFM_ID_2;
				else state = State::Header;
				break;
			}
			case State::FFM_ID_2:
			{
				if (data == FFM_ID_2) state = State::FFM_Hour;
				else state = State::Header;
				break;
			}
			case State::FFM_Hour:
			{
				mtc_buffer_.type = (data >> 5) & 0x03;
				mtc_buffer_.hour = (data >> 0) & 0x1F;
				state = State::FFM_Minute;
				break;
			}
			case State::FFM_Minute:
			{
				mtc_buffer_.minute = data;
				state = State::FFM_Second;
				break;
			}
			case State::FFM_Second:
			{
				mtc_buffer_.second = data;
				state = State::FFM_Frame;
				break;
			}
			case State::FFM_Frame:
			{
				mtc_buffer_.frame = data;
				state = State::FFM_EOX;
				break;
			}
			case State::FFM_EOX:
			{
				if (data == FFM_EOX)
				{
					mtc_packet_.push(mtc_buffer_);
				}
				else
				{
					ofLogError("MTCParser") << "invalid EOX";
				}
				state = State::Header;
				break;
			}

			// Quarter Frame Message
			case State::QFM_Value:
			{
				uint8_t index = (data >> 4) & 0x07;
				uint8_t value = (data >> 0) & 0x0F;

				switch (index)
				{
					case QFM_Index_Frame_LSB:
					{
						mtc_buffer_.frame = (value & 0x0F);
						break;
					}
					case QFM_Index_Frame_MSB:
					{
						mtc_buffer_.frame |= (value & 0x01) << 4;
						break;
					}
					case QFM_Index_Second_LSB:
					{
						mtc_buffer_.second = (value & 0x0F);
						break;
					}
					case QFM_Index_Second_MSB:
					{
						mtc_buffer_.second |= (value & 0x03) << 4;
						break;
					}
					case QFM_Index_Minute_LSB:
					{
						mtc_buffer_.minute = (value & 0x0F);
						break;
					}
					case QFM_Index_Minute_MSB:
					{
						mtc_buffer_.minute |= (value & 0x03) << 4;
						break;
					}
					case QFM_Index_Hour_LSB:
					{
						mtc_buffer_.hour = (value & 0x0F);
						break;
					}
					case QFM_Index_Hour_MSB:
					{
						mtc_buffer_.hour |= (value & 0x01) << 4;
						mtc_buffer_.type = (value >> 1) & 0x03;
						mtc_packet_.push(mtc_buffer_);
						clearBuffer();
						break;
					}
				}
				state = State::Header;
				break;
			}
		}
	}

private:

	void clearBuffer()
	{
		mtc_buffer_.type = 0xFF;
		mtc_buffer_.hour = 0xFF;
		mtc_buffer_.minute = 0xFF;
		mtc_buffer_.second = 0xFF;
		mtc_buffer_.frame = 0xFF;
	}
	
	enum class State
	{
		// for both
		Header,
		// for FFM only
		FFM_Header_2,
		FFM_Channel,
		FFM_ID_1,
		FFM_ID_2,
		FFM_Hour,
		FFM_Minute,
		FFM_Second,
		FFM_Frame,
		FFM_EOX,
		// for QFM only
		QFM_Value
	};
	enum StateValues
	{
		FFM_Header_1 = 0xF0,
		FFM_Header_2 = 0x7F,
		FFM_Channel = 0x7F,
		FFM_ID_1 = 0x01,
		FFM_ID_2 = 0x01,
		FFM_EOX = 0xF7,
		
		QFM_Header = 0xF1,
		QFM_Index_Frame_LSB = 0x00,
		QFM_Index_Frame_MSB = 0x01,
		QFM_Index_Second_LSB = 0x02,
		QFM_Index_Second_MSB = 0x03,
		QFM_Index_Minute_LSB = 0x04,
		QFM_Index_Minute_MSB = 0x05,
		QFM_Index_Hour_LSB = 0x06,
		QFM_Index_Hour_MSB = 0x07,
	};
	
	enum MTCType { FPS_24, FPS_25, FPS_29_97, FPS_30 };
	const float MTCFrameRate[4] { 24.f, 25.f, 29.97f, 30.f };
	const float MTCFrameSecond[4]
	{
		1.f / MTCFrameRate[0],
		1.f / MTCFrameRate[1],
		1.f / MTCFrameRate[2],
		1.f / MTCFrameRate[3],
	};
	
	RingQueue<MTCPacket> mtc_packet_ {3};
	MTCPacket mtc_buffer_;
	State state;
};


#endif
