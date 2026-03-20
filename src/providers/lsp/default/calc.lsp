;; calc.lsp
;;
;; Copyright 2026 Eva M
;;
;; This program is free software: you can redistribute it and/or modify
;; it under the terms of the GNU General Public License as published by
;; the Free Software Foundation, either version 3 of the License, or
;; (at your option) any later version.
;;
;; This program is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;; GNU General Public License for more details.
;;
;; You should have received a copy of the GNU General Public License
;; along with this program.  If not, see <https://www.gnu.org/licenses/>.
;;
;; SPDX-License-Identifier: GPL-3.0-or-later

(defun parse-tokens (str)
  (let ((skip 0))
    (labels ((handle-digit (ch idx)
               (parse-integer
                (concatenate
                 'string
                 (append (list ch)
                         (loop for scan from 0
                               for digit across (subseq str (1+ idx))
                               while (digit-char-p digit)
                               collect digit
                               finally (incf skip scan))))))
             (recurse (ch idx)
               (parse-tokens
                (let* ((stack (list idx))
                       (matching-paren nil))
                  (loop for scan from 0
                        for subch across (subseq str (1+ idx))
                        while stack
                        when (member subch '(#\())
                          do (push (+ idx scan 1) stack)
                        when (member subch '(#\)))
                          do (pop stack)
                        finally (progn
                                  (setf matching-paren (+ idx scan))
                                  (incf skip scan)))
                  (subseq str (1+ idx) matching-paren))))
             (handle-paren (ch idx)
               (let ((tokens (recurse ch idx)))
                 (unless tokens
                   (error "malformed expression"))
                 tokens)))
      (loop for ch across str
            for idx from 0
            unless (or (when (> skip 0)
                         (decf skip))
                       (member ch '(#\ )))
              collect
              (cond
                ((equal ch #\+) #'+)
                ((equal ch #\-) #'-)
                ((equal ch #\*) #'*)
                ((equal ch #\/) #'/)
                ((equal ch #\%) #'mod)
                ((equal ch #\^) #'expt)
                ((digit-char-p ch) (handle-digit ch idx))
                ((equal ch #\() (handle-paren ch idx))
                (t (error "invalid expression")))))))

(defun calc-tokens (tokens)
  (if (= (length tokens) 1)
      (let ((result (car tokens)))
        (cond
          ((listp result)
           (calc-tokens result))
          (result result)
          (t (error "malformed tokens"))))
      (let ((operators nil)
            (numbers nil)
            (final-result nil))
        (loop for token in tokens
              for idx from 0
              if (evenp idx)
                collect (if (listp token)
                            (calc-tokens token)
                            token)
                  into l-numbers
              else
                collect (list token (/ (1- idx) 2)) into l-operators
              finally (setf operators l-operators
                            numbers l-numbers))
        (labels ((get-precedence (x)
                   (let ((op (first x)))
                     (cond
                       ((equal op #'+) 1)
                       ((equal op #'-) 1)
                       ((equal op #'*) 2)
                       ((equal op #'/) 2)
                       ((equal op #'mod) 2)
                       ((equal op #'expt) 3)
                       (t 0))))
                 (predicate (&rest args)
                   (apply #'> (mapcar #'get-precedence args))))
          (setf operators (sort operators #'predicate)))
        (loop for operator in operators
              do (destructuring-bind (op idx) operator
                   (labels ((test-num-b (a b) (numberp b))
                            (find-num-backwards (idx lis)
                              (find t lis :test #'test-num-b
                                          :end (1+ idx)
                                          :from-end t))
                            (find-num-forwards (idx lis)
                              (find t lis :test #'test-num-b
                                          :start (1+ idx))))
                     (let ((result (funcall op
                                            (find-num-backwards idx numbers)
                                            (find-num-forwards idx numbers))))
                       (setf (nth idx numbers) result
                             (nth (1+ idx) numbers) nil
                             final-result result)))))
        final-result)))

(defun evaluate (str)
  (calc-tokens (parse-tokens str)))

(gobject:define-gobject-subclass
    "SaturnCaclResult"
    calc-result
    (:superclass g:object
     :export t
     :interfaces ())
    nil)

;; PROVIDER IMPLEMENTATION

(defun query (provider object store)
  (let* ((str (gtk:string-object-string object))
         (number (ignore-errors (evaluate str))))
    (when number
      (saturn:submit-result (let ((result (make-instance 'calc-result)))
                              (setf (g:object-data result "number") number)
                              result)
                            store provider))))

(defun score (provider item query)
  100000000000)

(defun select (provider item query)
  (format t "selected the calc result!~%")
  nil)

(defun bind-list-item (provider item)
  (let* ((number (g:object-data item "number"))
         (start-label
           (saturn:make-widget
            'gtk:label
            (:props (:label (format nil "~a" number)
                     :xalign 0.0
                     :ellipsize :start
                     :hexpand t)
             :styles ("title-4"))))
         (end-label
           (saturn:make-widget
            'gtk:label
            (:props (:label "Calculation"
                     :xalign 1.0
                     :ellipsize :start
                     :margin-end 25)
             :styles ("subtitle"))))
         (box
           (saturn:make-widget
            'gtk:box
            (:props (:orientation :horizontal
                     :spacing 25))
            (lambda (x)
              (gtk:box-append x start-label)
              (gtk:box-append x end-label)))))
    box))

(defun bind-preview (provider item)
  (let* ((label
           (saturn:make-widget 'gtk:label
               (:props (:label (format nil "~a" (g:object-data item "number"))
                        :ellipsize :middle
                        :hexpand t)
                :styles ("monospace")))))
    label))
