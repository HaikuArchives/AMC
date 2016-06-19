; Handy functions
;(define (nl n) (if (> n 0) (begin (newline) (nl(- n 1)))))
;(define (error s) (display "ERROR: ") (display s) (newline))
(define say (lambda s (for-each display s) (newline)))
;(define (repeat f) (if (f) (repeat f))) 
;(define (for a b f) (if (< a b) (begin (f a)(for (+ a 1) b f))))
