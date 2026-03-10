(require :asdf)

(defvar deps
  '(
    (:saturn-cl-deps       "./"                     )
    (:split-sequence       "./split-sequence/"      )
    (:closer-mop           "./closer-mop/"          )
    (:trivial-garbage      "./trivial-garbage/"     )
    (:global-vars          "./global-vars/"         )
    (:bordeaux-threads     "./bordeaux-threads/"    )
    (:iterate              "./iterate/"             )
    (:babel                "./babel/"               )
    (:alexandria           "./alexandria/"          )
    (:trivial-features     "./trivial-features/"    )
    (:cffi                 "./cffi/"                )
    (:cl-cffi-gtk4         "./cl-cffi-gtk4/"        )
    (:cl-cffi-gtk4-init    "./cl-cffi-gtk4/"        )
    (:cl-cffi-cairo        "./cl-cffi-cairo/"       )
    (:cl-cffi-glib         "./cl-cffi-glib/"        )
    (:cl-cffi-glib-init    "./cl-cffi-glib/"        )
    (:cl-cffi-pango        "./cl-cffi-pango/"       )
    (:cl-cffi-graphene     "./cl-cffi-graphene/"    )
    (:cl-cffi-gdk-pixbuf   "./cl-cffi-gdk-pixbuf/"  )
    ))

(mapcar #'(lambda (x)
            (push (merge-pathnames (second x))
                  asdf:*central-registry*))
        deps)
(mapcar #'(lambda (x)
            (let ((init-name
                    (concatenate 'string
                                 "init_lib_"
                                 (substitute #\_ #\- (string (first x))))))
              (asdf:make-build (first x)
                               :type :static-library
                               :move-here "./"
                               :init-name init-name)))
        deps)

(quit)
