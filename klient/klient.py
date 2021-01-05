import PySimpleGUI as sg
import socket
from time import sleep

def main():

    # All the stuff inside your window.
    layout = [
        [sg.Text('Enter username and password.')],
        [sg.Text('Username:', size=(15, 1)), sg.InputText()],
        [sg.Text('Password:', size=(15, 1)), sg.InputText(password_char='*')],
        [sg.Submit(), sg.Cancel()]
    ]

    # Create the Window
    window = sg.Window('SK2_Komunikator', layout)

    event, values = window.read()
    window.close()
    print(list(values.values()))     # masło.maślane()

    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect(('192.168.1.64', 1124))
    s.send(list(values.values())[0].encode())
    sleep(1)
    s.send(list(values.values())[1].encode())

    '''
    # Event Loop to process "events" and get the "values" of the inputs
    while True:
        event, values = window.read()
        if event == sg.WIN_CLOSED or event == 'sss':  # if user closes window or clicks cancel
            break

    window.close()
    return
    '''


if __name__ == "__main__":
    main()
