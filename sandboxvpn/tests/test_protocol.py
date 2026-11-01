import pytest
from sandboxvpn.protocol import Kind, pack, unpack, MAX_PAYLOAD

@pytest.mark.parametrize('kind',list(Kind))
def test_roundtrip(kind):
    for data in (b'',b'abc',b'\0'*MAX_PAYLOAD):
        assert unpack(pack(kind,123,data)) == (kind,123,data)

@pytest.mark.parametrize('data',[b'hello',b'\x01\x00\x00\x00'])
def test_invalid_packet(data):
    with pytest.raises(ValueError):unpack(data)

def test_bad_size():
    with pytest.raises(ValueError):unpack(pack(Kind.UDP_DATA,123,b'abc')[:-1])
    with pytest.raises(ValueError):pack(Kind.UDP_DATA,1,b'X'*(MAX_PAYLOAD+1))

