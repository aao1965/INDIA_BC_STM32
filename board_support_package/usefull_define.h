
#ifndef USEFULL_DEFINE_H_
#define USEFULL_DEFINE_H_

#define		ON		true
#define		OFF		false


#define		B0		(1<<0)
#define		B1		(1<<1)
#define		B2		(1<<2)
#define		B3		(1<<3)
#define		B4		(1<<4)
#define		B5		(1<<5)
#define		B6		(1<<6)
#define		B7		(1<<7)
#define		B8		(1<<8)
#define		B9		(1<<9)
#define		B10		(1<<10)
#define		B11		(1<<11)
#define		B12		(1<<12)
#define		B13		(1<<13)
#define		B14		(1<<14)
#define		B15		(1<<15)
#define		B16		(1<<16)
#define		B17		(1<<17)
#define		B18		(1<<18)
#define		B19		(1<<19)
#define		B20		(1<<20)
#define		B21		(1<<21)
#define		B22		(1<<22)
#define		B23		(1<<23)
#define		B24		(1<<24)
#define		B25		(1<<25)
#define		B26		(1<<26)
#define		B27		(1<<27)
#define		B28		(1<<28)
#define		B29		(1<<29)
#define		B30		(1<<30)
#define		B31		(1<<31)


#define		_PI		3.14159265359f
#define		_2PI	(2*_PI)
#define		_PI2	(_PI/2.f)


#define		_RAD_2_DEGREE		(180.f/_PI)
#define		_ANGLE_1_SEC		(_PI/(180.f*3600.f))
#define		_ANGLE_10_SEC		10.f*_ANGLE_1_SEC
#define		_ANGLE_30_SEC		30.f*_ANGLE_1_SEC
#define		_ANGLE_40_SEC		40.f*_ANGLE_1_SEC
#define		_ANGLE_1_MIN		60.f*_ANGLE_1_SEC
#define		_ANGLE_5_MIN		5.f*_ANGLE_1_MIN
#define		_ANGLE_10_MIN		10.f*_ANGLE_1_MIN
#define		_ANGLE_30_MIN		30.f*_ANGLE_1_MIN
#define		_ANGLE_1_DEGREE		(_PI/180.f)
#define		_ANGLE_5_DEGREE		(5.f*_ANGLE_1_DEGREE)
#define		_ANGLE_10_DEGREE	(10.f*_ANGLE_1_DEGREE)
#define		_ANGLE_15_DEGREE	(15.f*_ANGLE_1_DEGREE)
#define		_ANGLE_20_DEGREE	(20.f*_ANGLE_1_DEGREE)
#define		_ANGLE_30_DEGREE	(30.f*_ANGLE_1_DEGREE)
#define		_ANGLE_45_DEGREE	(45.f*_ANGLE_1_DEGREE)
#define		_ANGLE_90_DEGREE	(90.f*_ANGLE_1_DEGREE)
#define		_ANGLE_180_DEGREE	_PI



typedef	union{
	float 		f;
	uint32_t	u32;
	uint16_t	u16[2];
	uint8_t		u8[4];
}t_float_uint;


#endif /* USEFULL_DEFINE_H_ */
