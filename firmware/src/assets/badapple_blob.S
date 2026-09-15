    .section .rdata,"dr"
    .globl kBadAppleBlob
    .p2align 4
kBadAppleBlob:
    .incbin "assets/badapple.ba1p"
    .globl kBadAppleBlobEnd
kBadAppleBlobEnd:
