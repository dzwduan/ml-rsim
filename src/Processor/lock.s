#ifdef sparc
	
	.section	".text", #alloc, #execinstr
	.align	4


!------------------------------------------------------------------------------
	.global get_lock
get_lock:
	set	1, %g1
	swap	[%o0], %g1
	cmp	%g1, 0
	bne,pn	%icc, get_lock
	nop

	retl
	membar	#MemIssue

	
!------------------------------------------------------------------------------
! clear lock
	.global clr_lock
clr_lock:	
	membar	#MemIssue
	retl
	st	%g0, [%o0]

#endif


	

#ifdef sgi
!==============================================================================

        .set    noreorder
        .set    noat

        .section .text

!------------------------------------------------------------------------------
	.ent	get_lock
	.globl	get_lock
get_lock:
	.frame	$sp, 0, $31
1:	ll	$2, 0($4)
	bnez	$2, 1b
	li	$2, 1

	sc	$2, 0($4)
	nop
	beqz	$2, 1b
	nop
	
	j	$31
	sync
	.end	get_lock


!------------------------------------------------------------------------------
	.ent	clr_lock
	.globl	clr_lock
clr_lock:
	.frame	$sp, 0, $31
	sync
	jr	$31
	sw	$0, 0($4)
	.end	clr_lock
#endif

#ifdef linux
.text
	.align 16
.globl get_lock
	.type	 get_lock,@function
get_lock:
	movl	 4(%esp), %eax
1:
	lock
	btsl	 $0, (%eax)
	jnae	 1b

 # Possible (likely?) that lock btsl above serializes anyway???)
 #	mfence			# though this compiles, when run, it
				# give an illegal instruction exception.
				# mfence is a P4 instruction...
 #	push	 %ebx		# ebx is callee save, eax, ecx and eax
				# are caller save (cpuid modifies all 4)
 #	cpuid			# Just a random serial instruction that
				# appears to work.  Should find a better
				# one.
 #	pop	 %ebx
 #	lfence
 #	sfence			# Is this strong enough???
				# May not be on P4 or Xeon???

	ret
.Lfe1:
	.size	 get_lock,.Lfe1-get_lock

.text
	.align 16
.globl clr_lock
	.type	 clr_lock,@function
clr_lock:
 #	mfence
 #	push	 %ebx
 #	cpuid
 #	pop	 %ebx
 #	lfence

 # previous
 #	sfence			# Is this strong enough???
				# May not be on P4 or Xeon???

	movl	 4(%esp), %eax

 # Could possibly do:
 #	lock and $0, (%eax)
 # or
 #	movl	 $0, %ecx
 #	xchg	 %ecx, (%eax)	# Implicitly locks...

 # does not need to lock except that this ensures all previous load and
 # stores have completed!
 # or for that matter:
	lock
	btcl	 $0, (%eax)

 #previous
 #	movl	 $0, (%eax)

	ret
.Lfe2:
	.size	 clr_lock,.Lfe2-clr_lock
#endif
