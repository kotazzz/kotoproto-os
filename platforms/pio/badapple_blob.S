    .section .rodata
    .global kBadAppleBlob
    .align 4
kBadAppleBlob:
    .incbin "assets/badapple.ba1p"
    .global kBadAppleBlobEnd
kBadAppleBlobEnd:
