; Usage:
;    amc-miniscm add-chords.scm
; (AMC should be running)
; Notes in list received from AMC coded as:
;   (line nr, section nr, sign, duration)

(define (go) (load "add-chords.scm"))
(load "init2.scm")
(define amc-data #f)
(define amc-data (list>list `(get-selected-notes)))
(if (not amc-data) (quit))
(say "received: " amc-data)
(define meter (cadr (assq 'meter (car amc-data))))
(define notes (cdr amc-data))
(define (sh n) (- n (remainder n meter)))
(define (n1 n) (list (+(car n) 5)   (sh(cadr n))    (caddr n) 2))
(define (n2 n) (list (+(car n) 3) (+(sh(cadr n)) 2) (caddr n) 2))
(define (n3 n) (list (+(car n) 5) (+(sh(cadr n)) 4) (caddr n) 2))
(define (n4 n) (list (+(car n) 7) (+(sh(cadr n)) 6) (caddr n) 2))
(define new-notes (append (map n1 notes) (map n2 notes) (map n3 notes) (map n4 notes)))
(say "new notes: " new-notes)
(list>undef `(send-notes ,new-notes))
