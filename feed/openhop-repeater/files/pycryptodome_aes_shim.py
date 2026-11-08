"""Minimal pycryptodome-compatible AES (ECB only) for openhop_core on OpenWrt.

openhop_core only needs ``Crypto.Cipher.AES.new(key, AES.MODE_ECB).encrypt/decrypt``.
pycryptodome isn't packaged for OpenWrt and python3-cryptography drags in a Rust
toolchain, so this calls OpenSSL's libcrypto (already present for python3-openssl)
through ctypes. Falls back to `cryptography` if libcrypto can't be loaded.
"""
import ctypes
import ctypes.util

MODE_ECB = 1
block_size = 16
key_size = (16, 24, 32)

_lib = None
for _name in (ctypes.util.find_library("crypto"), "libcrypto.so.3", "libcrypto.so"):
    if not _name:
        continue
    try:
        _lib = ctypes.CDLL(_name)
        break
    except OSError:
        continue

if _lib is not None:
    _vp, _cp, _ip = ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int
    _lib.EVP_CIPHER_CTX_new.restype = _vp
    _lib.EVP_CIPHER_CTX_free.argtypes = [_vp]
    for _n in ("EVP_aes_128_ecb", "EVP_aes_192_ecb", "EVP_aes_256_ecb"):
        getattr(_lib, _n).restype = _vp
    _lib.EVP_CipherInit_ex.argtypes = [_vp, _vp, _vp, _cp, _cp, _ip]
    _lib.EVP_CIPHER_CTX_set_padding.argtypes = [_vp, _ip]
    _lib.EVP_CipherUpdate.argtypes = [_vp, _vp, ctypes.POINTER(_ip), _cp, _ip]
    _lib.EVP_CipherFinal_ex.argtypes = [_vp, _vp, ctypes.POINTER(_ip)]
    _CIPHERS = {16: _lib.EVP_aes_128_ecb, 24: _lib.EVP_aes_192_ecb, 32: _lib.EVP_aes_256_ecb}

    def _run(key, data, enc):
        key, data = bytes(key), bytes(data)
        if len(key) not in _CIPHERS:
            raise ValueError("Incorrect AES key length (%d bytes)" % len(key))
        if len(data) % 16:
            raise ValueError("Data must be aligned to block boundary in ECB mode")
        ctx = _lib.EVP_CIPHER_CTX_new()
        if not ctx:
            raise MemoryError("EVP_CIPHER_CTX_new failed")
        try:
            if _lib.EVP_CipherInit_ex(ctx, _CIPHERS[len(key)](), None, key, None, 1 if enc else 0) != 1:
                raise ValueError("EVP_CipherInit_ex failed")
            _lib.EVP_CIPHER_CTX_set_padding(ctx, 0)
            out = ctypes.create_string_buffer(len(data) + 16)
            n, m = ctypes.c_int(0), ctypes.c_int(0)
            if _lib.EVP_CipherUpdate(ctx, ctypes.addressof(out), ctypes.byref(n), data, len(data)) != 1:
                raise ValueError("EVP_CipherUpdate failed")
            if _lib.EVP_CipherFinal_ex(ctx, ctypes.addressof(out) + n.value, ctypes.byref(m)) != 1:
                raise ValueError("EVP_CipherFinal_ex failed")
            return out.raw[: n.value + m.value]
        finally:
            _lib.EVP_CIPHER_CTX_free(ctx)
else:  # pragma: no cover - fallback for hosts with `cryptography` but no loadable libcrypto
    from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes

    def _run(key, data, enc):
        c = Cipher(algorithms.AES(bytes(key)), modes.ECB())
        x = c.encryptor() if enc else c.decryptor()
        return x.update(bytes(data)) + x.finalize()


class _ECB:
    def __init__(self, key):
        self._key = bytes(key)

    def encrypt(self, data):
        return _run(self._key, data, True)

    def decrypt(self, data):
        return _run(self._key, data, False)


def new(key, mode, *args, **kwargs):
    if mode != MODE_ECB:
        raise NotImplementedError("shim supports AES.MODE_ECB only")
    return _ECB(key)
