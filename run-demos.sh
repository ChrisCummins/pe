# List of demonstrations
demos=(fountains fireworks)

# Time to run each demonstration for
time=30

(
	echo "Beginning ${demos[$?]} demonstrations"

	# Run in sub-shell to suppress stderr garbage from kill
	for d in ${demos[*]}; do
		echo "Running demonstration ./$d for $time seconds..."
		(cmdpid=$BASHPID; (sleep $time; kill $cmdpid) & exec ./$d)
	done

	echo "Demonstrations complete"
) 2>/dev/null
