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
(defparameter *default-task-stack-size* 256)

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

(defun emit-define (objects)
  (format t "#define NR_TASK ~a~%" (1+ (number-of "tasks" objects)))
  (emit-define-id (getvalue objects "tasks"))
  (terpri)
  (emit-define-id (getvalue objects "events") #'(lambda (n) (ash 1 n)))
  (terpri)
  (format t "#define NR_RES ~a~%" (number-of "resources" objects))
  (emit-define-id (getvalue objects "resources"))
  (terpri)
  (format t "#define NR_ALARM ~a~%" (number-of "alarms" objects))
  (emit-define-id (getvalue objects "alarms")))

(defun emit-object-declaration (objects type id to-s)
  (format t "const ~a ~a[] = {~%~{    {~{~a~^, ~}}~^,~%~}~%};~%"
          type id (mapcar #'(lambda (object)
                              (funcall to-s object)) objects)))

(defun emit-task-declaration (tasks)
  (emit-object-declaration tasks "task_rom_t" "task_rom"
    (let ((acc 0))
      #'(lambda (object)
          (prog1
              (list (getvalue object "name")
                    (getvalue object "pri")
                    (format nil "user_task_stack + USER_TASK_STACK_SIZE - ~a" acc)
                    (if (getvalue object "autostart") "TRUE" "FALSE"))
            (incf acc (getvalue object "stack_size")))))))

(defun emit-resource-declaration (resources)
  (emit-object-declaration resources "res_rom_t" "res_rom"
    #'(lambda (object)
        (list (getvalue object "pri")))))

(defun emit-alarm-declaration (alarms)
  (emit-object-declaration alarms "alarm_action_rom_t" "alarm_action_rom"
    #'(lambda (object)
        (let* ((action (getvalue object "action"))
               (type (getvalue action "type"))
               (enum (format nil "ACTION_TYPE_~a" type)))
          (cond ((string= type "ACTIVATETASK")
                 (list enum
                       (format nil "{{~a, ~a}}"
                               (string-upcase (getvalue action "task"))
                               0)))
                ((string= type "SETEVENT")
                 (list enum
                       (format nil "{{~a, ~a}}"
                               (string-upcase (getvalue action "task"))
                               (string-upcase (getvalue action "event")))))
                ((string= type "ALARMCALLBACK")
                 (list enum
                       (format nil "{{(int)~a, ~a}}"
                               (getvalue action "callback")
                               0))))))))

(defun emit-header (objects)
  (let ((macro (string-upcase (substitute #\_ #\. (file-namestring *h-file*)))))
    (format t "#ifndef ~a~%#define ~a~%~%" macro macro)
    (format t "#include \"uros.h\"~%~%"))
  (emit-define objects)
  (terpri)
  (mapc #'(lambda (task) (format t "void ~a(int ex);~%" (getvalue task "name"))) 
        (getvalue objects "tasks"))
  (terpri)
  (format t "void ~a(void);~%" "main_task_callback")
  (terpri)
  (format t "extern const task_rom_t task_rom[];~%")
  (format t "extern const res_rom_t res_rom[];~%")
  (format t "extern const alarm_action_rom_t alarm_action_rom[];~%")
  (format t "~%#endif"))

(defun emit-source (objects)
  (format t "#include \"~a\"~%~%" (file-namestring *h-file*))
  (format t "extern uint32_t user_task_stack[];~%~%")
  (emit-task-declaration (getvalue objects "tasks"))
  (terpri)
  (emit-resource-declaration (getvalue objects "resources"))
  (terpri)
  (emit-alarm-declaration (getvalue objects "alarms")))

(defun exit-on-error (message)
  (format t message)
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
    (exit-on-error "JSON file is not specified as an argument.~%"))
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
      (exit-on-error "JSON file is not specified as an argument.~%"))
    (let ((filename (cl-fad:pathname-as-file (car argv)))
          objects)
      (handler-case
          (setf objects (jsown:parse (alexandria:read-file-into-string filename)))
        (error (condition)
          (declare (ignore condition))
          (exit-on-error "Error: Invalid JSON format~%")))
      (setf objects
            (insert-task `(:OBJ
                           ("name" . "default_task")
                           ("pri" . "PRI_MAX")
                           ("stack_size" . ,*default-task-stack-size*)
                           ("autostart" . t))
                         objects))
      (with-open-file (*standard-output* h-file :direction :output :if-exists :supersede)
        (handler-case
            (emit-header objects)
          (error (condition)
            (declare (ignore condition))
            (exit-on-error "Error: Invalid object structure~%"))))
      (format t "Create ~a~%" (namestring h-file))
      (with-open-file (*standard-output* c-file :direction :output :if-exists :supersede)
        (handler-case
            (emit-source objects)
          (error (condition)
            (declare (ignore condition))
            (exit-on-error "Error: Invalid object structure~%"))))
      (format t "Create ~a~%" (namestring c-file)))))
