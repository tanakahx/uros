#|
exec ros -Q -- $0 "$@"
|#

(in-package :cl-user)

(eval-when (:compile-toplevel :load-toplevel :execute)
  (ql:quickload '(:alexandria :cl-fad :jsown) :silent t))

(defpackage :config
  (:use :cl)
  (:import-from :alexandria
                :iota
                :read-file-into-string
                :write-string-into-file)
  (:import-from :cl-fad
                :pathname-as-directory
                :pathname-as-file)
  (:import-from :jsown
                :parse))

(in-package :config)

(defparameter *directory* (cl-fad:pathname-as-directory "."))
(defparameter *c-file* (cl-fad:pathname-as-file "config.c"))
(defparameter *h-file* (cl-fad:pathname-as-file "config.h"))

(defun getvalue (object key)
  (cdr (find-if #'(lambda (m) (equal (car m) key)) (cdr object))))

(defun number-of (type objects)
  (length (getvalue objects type)))

(defun emit-define-id (objects &optional (assign #'identity))
  (let* ((names (mapcar #'cdadr objects))
         (ids (mapcar assign (alexandria:iota (length objects))))
         (zip (mapcar #'cons names ids)))
    (mapc #'(lambda (pair)
              (format t "#define ~a ~a~%" (string-upcase (car pair)) (cdr pair)))
          zip)))

(defun emit-header (objects)
  (format t "#define NR_TASK ~a~%" (1+ (number-of "tasks" objects)))
  (emit-define-id (getvalue objects "tasks") #'1+)
  (terpri)
  (emit-define-id (getvalue objects "events") #'(lambda (n) (ash 1 n)))
  (terpri)
  (format t "#define NR_RES ~a~%" (number-of "resources" objects))
  (emit-define-id (getvalue objects "resources"))
  (terpri)
  (format t "#define NR_ALARM ~a~%" (number-of "alarms" objects))
  (emit-define-id (getvalue objects "alarms")))

(defun emit-object (to-s object)
  (format nil "{~{~a~^, ~}}" (funcall to-s object)))

(defun emit-object-declaration (objects type id to-s)
  (format t "const struct ~a ~a[] = {~%~{    ~a~^,~%~}~%};~%"
          type id (mapcar #'(lambda (object)
                              (emit-object to-s object)) objects)))

(defun emit-task-declaration (tasks)
  (emit-object-declaration tasks "task_rom_t" "task_rom"
    (let ((acc 0))
      (lambda (object)
        (prog1
            (list (getvalue object "name")
                  (getvalue object "pri")
                  (format nil "user_task_stack + USER_TASK_STACK_SIZE - (~a + ~a)"
                          acc (getvalue object "stack_size"))
                  (if (getvalue object "autostart") "TRUE" "FALSE"))
          (incf acc (getvalue object "stack_size")))))))

(defun emit-resource-declaration (resources)
  (emit-object-declaration resources "resource_rom_t" "resource_rom"
    (lambda (object)
      (list (getvalue object "pri")))))

(defun emit-alarm-declaration (alarms)
  (emit-object-declaration alarms "alarm_rom_t" "alarm"
    (lambda (object)
      (let* ((action (getvalue object "action"))
             (type (getvalue action "type")))
        (cond ((string= type "ACTIVATETASK")
               (list type
                     (format nil "{{~a, ~a}}"
                             (string-upcase (getvalue action "task"))
                             0)))
              ((string= type "SETEVENT")
               (list type
                     (format nil "{{~a, ~a}}"
                             (string-upcase (getvalue action "task"))
                             (string-upcase (getvalue action "event")))))
              ((string= type "ALARMCALLBACK")
               (list type
                     (format nil "{{(int)~a, ~a}}"
                             (getvalue action "callback")
                             0))))))))

(defun exit-on-file-not-specified ()
  (format t "JSON file is not specified as an argument.~%")
  (uiop:quit 1))

(defun insert-task (task objects)
  (labels ((rec (lst)
             (if lst
                 (if (equal (caar lst) "tasks")
                     (cons (cons "tasks" (cons task (cdar lst)))
                           (cdr lst))
                     (cons (car lst)
                           (rec (cdr lst)))))))
    (cons :OBJ (rec (cdr objects)))))

(defun main (&rest argv)
  (when (< (length argv) 1)
    (exit-on-file-not-specified))
  (let ((directory *directory*)
        (h-file *h-file*)
        (c-file *c-file*))
    (when (and (equal (car argv) "-d")
               (>= (length (cdr argv)) 1))
      (setf directory (cl-fad:pathname-as-directory (cadr argv))
            h-file (cl-fad:merge-pathnames-as-file directory *h-file*)
            c-file (cl-fad:merge-pathnames-as-file directory *c-file*)
            argv (cddr argv)))
    (when (/= (length argv) 1)
      (exit-on-file-not-specified))
    (let ((filename (cl-fad:pathname-as-file (car argv))))
      (handler-case
          (let ((objects (jsown:parse (alexandria:read-file-into-string filename))))
            (setf objects
                  (insert-task
                   '(:OBJ ("name" . "default_task") ("pri" . "PRI_MAX") ("stack_size" . 64) ("autostart" . t))
                   objects))
            (handler-case
                (progn
                  (with-open-file (*standard-output* h-file :direction :output :if-exists :supersede)
                    (let ((macro (string-upcase (substitute #\_ #\. (file-namestring *h-file*)))))
                      (format t "\"uros.h\"")
                      (format t "#ifndef ~a~%#define ~a~%~%" macro macro))
                    (emit-header objects)
                    (terpri)
                    (format t "extern const task_rom_t task_rom[];~%")
                    (format t "extern const res_rom_t res_rom[];~%")
                    (format t "extern const alarm_action_rom_t alarm_action_rom[];~%")
                    (format t "~%#endif"))
                  (format t "Create ~a~%" (namestring h-file))
                  (with-open-file (*standard-output* c-file :direction :output :if-exists :supersede)
                    (format t "#include \"~a\"~%~%" (file-namestring *h-file*))
                    (format t "extern uint32_t user_task_stack[];~%~%")
                    (emit-task-declaration (getvalue objects "tasks"))
                    (terpri)
                    (emit-resource-declaration (getvalue objects "resources"))
                    (terpri)
                    (emit-alarm-declaration (getvalue objects "alarms")))
                  (format t "Create ~a~%" (namestring c-file)))
              (error (condition)
                (declare (ignore condition))
                (format *error-output* "Error: Invalid object structure~%")
                (uiop:quit 1))))
        (error (condition)
          (declare (ignore condition))
          (format *error-output* "Error: Invalid JSON format~%")
          (uiop:quit 1))))))
