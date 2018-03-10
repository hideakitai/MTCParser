# MTCParser
Midi Time Code (MTC) parser


## Feature

- parse Midi Time Code protocol
- support Quarter Frame Message (QFM) and Full Frame Message (FFM)
- supported frame rate is ```24``` / ```25``` / ```29.97``` / ```30```


## Dependencies

None


## Usage

``` c++
uint8_t* data = getMidiBinaryDataPointer();
uint8_t size = getMidiBinaryDataSize();

MTCParser mtc;
mtc.feed(data, size);

while (mtc.available())
{
	// get each time code as uint8_t
	mtc.type();
	mtc.hour();
	mtc.minute();
	mtc.second();
	mtc.frame();
	
	// get current time as :
	mtc.asFrameCount();
	mtc.asSeconds();
	mtc.asString();
	
	// finally go to next time code
	mtc.pop();
}
```


## License

MIT


## Acknowledgments / Contributor

- [ISHII 2bit](https://github.com/2bbb)


