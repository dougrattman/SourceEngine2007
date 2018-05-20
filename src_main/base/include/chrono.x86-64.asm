
.code

; u64 start_tsc()
start_tsc proc
  ; Warm up CPU instruction cache (3 times).
  push   rbx
  cpuid           ; clobber rax, rbx, rcx, rdx
  rdtsc           ; rdx has high part, rax has low part
  shl    rdx,20h  ; rdx << 32
  or     rax,rdx  ; rax = rdx << 32 | rax
  pop    rbx

  push   rbx
  rdtscp          ; rdx has high part, rax has low part
  shl    rdx,20h  ; rdx << 32
  or     rax,rdx  ; rax = rdx << 32 | rax
  push   rax      ; rax has output TSC.
  cpuid           ; clobber rax, rbx, rcx, rdx
  pop    rax
  pop    rbx

  push   rbx
  cpuid           ; clobber rax, rbx, rcx, rdx
  rdtsc           ; rdx has high part, rax has low part
  shl    rdx,20h  ; rdx << 32
  or     rax,rdx  ; rax = rdx << 32 | rax
  pop    rbx

  push   rbx
  rdtscp          ; rdx has high part, rax has low part
  shl    rdx,20h  ; rdx << 32
  or     rax,rdx  ; rax = rdx << 32 | rax
  push   rax      ; rax has output TSC.
  cpuid           ; clobber rax, rbx, rcx, rdx
  pop    rax
  pop    rbx

  push   rbx
  cpuid           ; clobber rax, rbx, rcx, rdx
  rdtsc           ; rdx has high part, rax has low part
  shl    rdx,20h  ; rdx << 32
  or     rax,rdx  ; rax = rdx << 32 | rax
  pop    rbx

  push   rbx
  rdtscp          ; rdx has high part, rax has low part
  shl    rdx,20h  ; rdx << 32
  or     rax,rdx  ; rax = rdx << 32 | rax
  push   rax      ; rax has output TSC.
  cpuid           ; clobber rax, rbx, rcx, rdx
  pop    rax
  pop    rbx

  ; Measure.
  push   rbx
  cpuid           ; clobber rax, rbx, rcx, rdx
  rdtsc           ; rdx has high part, rax has low part
  shl    rdx,20h  ; rdx << 32
  or     rax,rdx  ; rax = rdx << 32 | rax
  pop    rbx
  ret
start_tsc endp

; u64 end_tsc()
end_tsc proc
  push   rbx
  rdtscp          ; rdx has high part, rax has low part
  shl    rdx,20h  ; rdx << 32
  or     rax,rdx  ; rax = rdx << 32 | rax
  push   rax      ; rax has output TSC.
  cpuid           ; clobber rax, rbx, rcx, rdx
  pop    rax
  pop    rbx
  ret
end_tsc endp

end
